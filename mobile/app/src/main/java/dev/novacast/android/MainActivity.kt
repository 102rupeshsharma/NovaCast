package dev.novacast.android

import android.os.Build
import android.os.Bundle
import android.os.PowerManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.compose.runtime.getValue
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.compose.LifecycleEventEffect
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import dev.novacast.android.ui.MainScreen
import dev.novacast.android.ui.MainViewModel
import dev.novacast.android.ui.NovaCastTheme

class MainActivity : ComponentActivity() {
    private val viewModel: MainViewModel by viewModels()
    private var thermalListener: PowerManager.OnThermalStatusChangedListener? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            NovaCastTheme {
                val state by viewModel.state.collectAsStateWithLifecycle()
                // Storage Access Framework: no storage permission is needed.
                val pickBios = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
                    if (uri != null) viewModel.loadBios(uri)
                }
                LifecycleEventEffect(Lifecycle.Event.ON_STOP) { viewModel.onAppBackgrounded() }
                MainScreen(
                    state = state,
                    onPickBios = { pickBios.launch(arrayOf("*/*")) },
                    onForgetBios = viewModel::forgetBios,
                    onStart = viewModel::start,
                    onPause = viewModel::pause,
                    onStop = viewModel::stop,
                    onDismissMessage = viewModel::dismissMessage,
                )
            }
        }
    }

    override fun onStart() {
        super.onStart()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            val power = getSystemService(PowerManager::class.java)
            val listener = PowerManager.OnThermalStatusChangedListener { status ->
                if (status >= PowerManager.THERMAL_STATUS_SEVERE) viewModel.onThermalThrottling()
            }
            power.addThermalStatusListener(mainExecutor, listener)
            thermalListener = listener
        }
    }

    override fun onStop() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            thermalListener?.let { getSystemService(PowerManager::class.java).removeThermalStatusListener(it) }
            thermalListener = null
        }
        super.onStop()
    }
}
