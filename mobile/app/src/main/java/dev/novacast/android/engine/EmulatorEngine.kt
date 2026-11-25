package dev.novacast.android.engine

import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.withContext
import java.util.concurrent.Executors

data class ModuleInfo(val id: String, val status: String, val hardware: String, val notes: String)

data class EngineSnapshot(
    val cycles: Long,
    val instructions: Long,
    val pc: Long,
    val pr: Long,
    val sr: Long,
    val sleeping: Boolean,
    val unmappedAccesses: Long,
) {
    val emulatedSeconds: Double get() = cycles / CYCLES_PER_SECOND

    companion object {
        const val CYCLES_PER_SECOND = 200_000_000.0
        val EMPTY = EngineSnapshot(0, 0, 0, 0, 0, false, 0)
    }
}

/**
 * Owns one native emulator instance and confines every call to it to a
 * single dedicated thread, so the C++ engine is never used concurrently.
 */
class EmulatorEngine : AutoCloseable {
    private val executor = Executors.newSingleThreadExecutor { runnable ->
        Thread(runnable, "novacast-engine").apply { priority = Thread.NORM_PRIORITY }
    }

    /** The engine thread. Long-running emulation loops run here too. */
    val dispatcher: CoroutineDispatcher = executor.asCoroutineDispatcher()

    private var handle = 0L

    suspend fun open() = withContext(dispatcher) {
        if (handle == 0L) handle = NativeBridge.create()
    }

    /** Must be called on [dispatcher]. */
    fun requireHandle(): Long {
        check(handle != 0L) { "engine is not open" }
        return handle
    }

    suspend fun version(): String = withContext(dispatcher) { NativeBridge.version() }

    suspend fun modules(): List<ModuleInfo> = withContext(dispatcher) {
        NativeBridge.moduleStatus(requireHandle()).map { row ->
            val parts = row.split('\t')
            ModuleInfo(parts[0], parts[1], parts.getOrElse(2) { "" }, parts.getOrElse(3) { "" })
        }
    }

    suspend fun canRunSoftware(): Boolean = withContext(dispatcher) { NativeBridge.canRunSoftware(requireHandle()) }

    suspend fun loadBios(image: ByteArray): String? = withContext(dispatcher) {
        NativeBridge.loadBios(requireHandle(), image)
    }

    suspend fun reset() = withContext(dispatcher) { NativeBridge.reset(requireHandle()) }

    suspend fun snapshot(): EngineSnapshot = withContext(dispatcher) { snapshotNow() }

    /** Runs [cycles] on the calling thread, which must be the engine thread. */
    fun runNow(cycles: Long): String? = NativeBridge.run(requireHandle(), cycles)

    fun snapshotNow(): EngineSnapshot {
        val v = NativeBridge.snapshot(requireHandle())
        return EngineSnapshot(v[0], v[1], v[2], v[3], v[4], v[5] != 0L, v[6])
    }

    override fun close() {
        executor.execute {
            if (handle != 0L) {
                NativeBridge.destroy(handle)
                handle = 0L
            }
        }
        executor.shutdown()
    }
}
