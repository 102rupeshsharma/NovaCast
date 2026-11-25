package dev.novacast.android.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AssistChip
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import dev.novacast.android.engine.ModuleInfo

@Composable
fun MainScreen(
    state: UiState,
    onPickBios: () -> Unit,
    onForgetBios: () -> Unit,
    onStart: () -> Unit,
    onPause: () -> Unit,
    onStop: () -> Unit,
    onDismissMessage: () -> Unit,
) {
    Surface(modifier = Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background) {
        Column(
            modifier = Modifier
                .safeDrawingPadding()
                .verticalScroll(rememberScrollState())
                .padding(16.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            // Phones are narrow; tablets and landscape get a centred column.
            Column(modifier = Modifier.widthIn(max = 720.dp).fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Header(state.version)
                state.message?.let { MessageCard(it, onDismissMessage) }
                BiosCard(state, onPickBios, onForgetBios)
                EmulationCard(state, onStart, onPause, onStop)
                GamesCard()
                ModulesCard(state.modules)
                Text(
                    "NovaCast contains no Sega BIOS or games. Use only a boot ROM dumped from a console you own.",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
    }
}

@Composable
private fun Header(version: String) {
    Column {
        Text("NovaCast", style = MaterialTheme.typography.headlineMedium, fontWeight = FontWeight.Bold)
        Text(
            if (version.isEmpty()) "Starting engine…" else "Version $version · pre-alpha",
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        Spacer(Modifier.padding(4.dp))
        Text(
            "Early development build: games cannot be played yet. Only the SH-4 CPU and memory map are emulated.",
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.primary,
        )
    }
}

@Composable
private fun MessageCard(message: String, onDismiss: () -> Unit) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Row(modifier = Modifier.padding(12.dp), verticalAlignment = Alignment.CenterVertically) {
            Text(message, modifier = Modifier.weight(1f), style = MaterialTheme.typography.bodyMedium)
            TextButton(onClick = onDismiss) { Text("OK") }
        }
    }
}

@Composable
private fun SectionCard(title: String, content: @Composable () -> Unit) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text(title, style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.SemiBold)
            content()
        }
    }
}

@Composable
private fun BiosCard(state: UiState, onPickBios: () -> Unit, onForgetBios: () -> Unit) {
    SectionCard("Boot ROM") {
        Text(
            state.biosName?.let { "Loaded: $it" } ?: "No boot ROM loaded. Select a 2 MB dc_boot.bin dumped from your own console.",
            style = MaterialTheme.typography.bodyMedium,
        )
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = onPickBios, enabled = state.runState != RunState.Starting) { Text("Select boot ROM") }
            if (state.biosName != null) OutlinedButton(onClick = onForgetBios) { Text("Forget") }
        }
    }
}

@Composable
private fun EmulationCard(state: UiState, onStart: () -> Unit, onPause: () -> Unit, onStop: () -> Unit) {
    SectionCard("Emulation") {
        val s = state.snapshot
        Text(
            when (state.runState) {
                RunState.Starting -> "Starting…"
                RunState.NoBios -> "Load a boot ROM to run the CPU."
                RunState.Ready -> "Ready (power-on state)."
                RunState.Running -> "Running the boot ROM on the SH-4 interpreter."
                RunState.Paused -> "Paused."
            },
            style = MaterialTheme.typography.bodyMedium,
        )
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            when (state.runState) {
                RunState.Running -> Button(onClick = onPause) { Text("Pause") }
                RunState.Paused -> Button(onClick = onStart) { Text("Resume") }
                else -> Button(onClick = onStart, enabled = state.runState == RunState.Ready) { Text("Start") }
            }
            OutlinedButton(
                onClick = onStop,
                enabled = state.runState == RunState.Running || state.runState == RunState.Paused,
            ) { Text("Stop") }
        }
        val mono = MaterialTheme.typography.bodySmall.copy(fontFamily = FontFamily.Monospace)
        Text("Emulated time  %.3f s".format(s.emulatedSeconds), style = mono)
        Text("Instructions   %,d".format(s.instructions), style = mono)
        Text("Speed          %.1f MIPS (real time = 200)".format(state.mips), style = mono)
        Text("PC=%08X PR=%08X SR=%08X".format(s.pc, s.pr, s.sr), style = mono)
        Text("Sleeping: ${if (s.sleeping) "yes" else "no"} · unmapped accesses: ${s.unmappedAccesses}", style = mono)
        Text(
            "A real boot ROM needs video, GD-ROM and timer hardware that is not emulated yet, so it stops making progress early.",
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
    }
}

@Composable
private fun GamesCard() {
    SectionCard("Games") {
        Text(
            "Loading GDI/CDI disc images is not implemented in the emulator core yet, on any platform.",
            style = MaterialTheme.typography.bodyMedium,
        )
        Button(onClick = {}, enabled = false) { Text("Load game image (not available yet)") }
    }
}

@Composable
private fun ModulesCard(modules: List<ModuleInfo>) {
    SectionCard("Hardware modules") {
        modules.forEach { m ->
            Column {
                Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text(m.id, style = MaterialTheme.typography.bodyLarge, fontWeight = FontWeight.Medium)
                    AssistChip(onClick = {}, label = { Text(m.status) })
                }
                Text(m.notes, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
    }
}
