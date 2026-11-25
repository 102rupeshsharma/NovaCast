package dev.novacast.android.ui

import android.app.Application
import android.net.Uri
import android.provider.OpenableColumns
import android.util.Log
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import dev.novacast.android.engine.EmulatorEngine
import dev.novacast.android.engine.EngineSnapshot
import dev.novacast.android.engine.ModuleInfo
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.coroutines.yield
import java.io.File

enum class RunState { Starting, NoBios, Ready, Running, Paused }

data class UiState(
    val version: String = "",
    val modules: List<ModuleInfo> = emptyList(),
    val canRunSoftware: Boolean = false,
    val biosName: String? = null,
    val runState: RunState = RunState.Starting,
    val snapshot: EngineSnapshot = EngineSnapshot.EMPTY,
    val mips: Double = 0.0,
    val message: String? = null,
)

class MainViewModel(application: Application) : AndroidViewModel(application) {
    private val engine = EmulatorEngine()
    private val _state = MutableStateFlow(UiState())
    val state: StateFlow<UiState> = _state.asStateFlow()

    private var runJob: Job? = null

    // The boot ROM is copied into app-private storage so it survives restarts
    // without needing any storage permission.
    private val savedBios = File(application.filesDir, "bios/dc_boot.bin")

    init {
        viewModelScope.launch {
            try {
                engine.open()
                _state.update {
                    it.copy(
                        version = engine.version(),
                        modules = engine.modules(),
                        canRunSoftware = engine.canRunSoftware(),
                        runState = RunState.NoBios,
                    )
                }
                if (savedBios.isFile) {
                    val bytes = withContext(Dispatchers.IO) { savedBios.readBytes() }
                    applyBios(bytes, "saved boot ROM", persist = false)
                }
            } catch (e: Exception) {
                Log.e(TAG, "engine start failed", e)
                _state.update { it.copy(message = "Emulator engine failed to start: ${e.message}") }
            }
        }
    }

    fun loadBios(uri: Uri) {
        pause()
        viewModelScope.launch {
            val resolver = getApplication<Application>().contentResolver
            val name = withContext(Dispatchers.IO) {
                resolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { c ->
                    if (c.moveToFirst()) c.getString(0) else null
                }
            } ?: "selected file"
            val bytes = try {
                withContext(Dispatchers.IO) {
                    resolver.openInputStream(uri)?.use { input ->
                        // Read at most one byte more than a boot ROM so oversize files are rejected cheaply.
                        val buffer = ByteArray(BIOS_SIZE + 1)
                        var total = 0
                        while (total < buffer.size) {
                            val n = input.read(buffer, total, buffer.size - total)
                            if (n < 0) break
                            total += n
                        }
                        buffer.copyOf(total)
                    }
                }
            } catch (e: Exception) {
                Log.w(TAG, "cannot read $uri", e)
                null
            }
            if (bytes == null) {
                _state.update { it.copy(message = "Could not read \"$name\".") }
                return@launch
            }
            applyBios(bytes, name, persist = true)
        }
    }

    private suspend fun applyBios(bytes: ByteArray, name: String, persist: Boolean) {
        val error = engine.loadBios(bytes)
        if (error != null) {
            _state.update { it.copy(message = "\"$name\" was not accepted: $error") }
            return
        }
        if (persist) {
            withContext(Dispatchers.IO) {
                savedBios.parentFile?.mkdirs()
                savedBios.writeBytes(bytes)
            }
        }
        engine.reset()
        _state.update {
            it.copy(
                biosName = name,
                runState = RunState.Ready,
                snapshot = engine.snapshot(),
                mips = 0.0,
                message = "Boot ROM loaded. Press Start to run the SH-4 CPU.",
            )
        }
    }

    fun forgetBios() {
        stop()
        viewModelScope.launch {
            withContext(Dispatchers.IO) { savedBios.delete() }
            _state.update { it.copy(biosName = null, runState = RunState.NoBios, message = "Saved boot ROM removed.") }
        }
    }

    /** Starts or resumes the emulation loop on the engine thread. */
    fun start() {
        val current = _state.value.runState
        if (current != RunState.Ready && current != RunState.Paused) return
        _state.update { it.copy(runState = RunState.Running, message = null) }
        runJob = viewModelScope.launch(engine.dispatcher) {
            var lastUpdate = System.nanoTime()
            var lastInstructions = engine.snapshotNow().instructions
            while (isActive) {
                val error = engine.runNow(SLICE_CYCLES)
                if (error != null) {
                    _state.update { it.copy(runState = RunState.Paused, message = "Emulation stopped: $error") }
                    break
                }
                val now = System.nanoTime()
                if (now - lastUpdate >= UI_UPDATE_NANOS) {
                    val snap = engine.snapshotNow()
                    val seconds = (now - lastUpdate) / 1e9
                    val mips = (snap.instructions - lastInstructions) / seconds / 1e6
                    _state.update { it.copy(snapshot = snap, mips = mips) }
                    lastUpdate = now
                    lastInstructions = snap.instructions
                }
                yield() // let queued engine calls (pause, snapshot) run between slices
            }
        }
    }

    fun pause() {
        val job = runJob ?: return
        runJob = null
        job.cancel()
        viewModelScope.launch {
            job.join()
            val snap = engine.snapshot()
            _state.update {
                // A paused CPU executes nothing, so the speed reading is 0, not the last sample.
                if (it.runState == RunState.Running) it.copy(runState = RunState.Paused, snapshot = snap, mips = 0.0) else it
            }
        }
    }

    /** Stops emulation and resets the system to its power-on state. */
    fun stop() {
        val job = runJob
        runJob = null
        job?.cancel()
        viewModelScope.launch {
            job?.join()
            if (_state.value.biosName == null) return@launch
            engine.reset()
            _state.update { it.copy(runState = RunState.Ready, snapshot = engine.snapshot(), mips = 0.0) }
        }
    }

    /** Called when the app leaves the foreground: never keep emulating in the background. */
    fun onAppBackgrounded() {
        if (_state.value.runState == RunState.Running) {
            pause()
            _state.update { it.copy(message = "Paused because NovaCast went to the background.") }
        }
    }

    /** Called by the activity when Android reports severe thermal pressure. */
    fun onThermalThrottling() {
        if (_state.value.runState == RunState.Running) {
            pause()
            _state.update { it.copy(message = "Paused because the device is overheating.") }
        }
    }

    fun dismissMessage() = _state.update { it.copy(message = null) }

    override fun onCleared() {
        runJob?.cancel()
        engine.close()
    }

    private companion object {
        const val TAG = "NovaCast"
        const val BIOS_SIZE = 2 * 1024 * 1024
        const val SLICE_CYCLES = 2_000_000L // 10 ms of emulated time per slice
        const val UI_UPDATE_NANOS = 250_000_000L
    }
}
