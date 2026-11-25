package dev.novacast.android.ui

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

private val NovaCastColors = darkColorScheme(
    primary = Color(0xFFFF7A45),
    onPrimary = Color(0xFF1A0E08),
    secondary = Color(0xFF7FB7FF),
    background = Color(0xFF12141C),
    surface = Color(0xFF1B1E28),
    surfaceVariant = Color(0xFF252935),
    onBackground = Color(0xFFE6E8EF),
    onSurface = Color(0xFFE6E8EF),
    onSurfaceVariant = Color(0xFFB4B8C5),
    error = Color(0xFFFF6B6B),
)

@Composable
fun NovaCastTheme(content: @Composable () -> Unit) {
    MaterialTheme(colorScheme = NovaCastColors, content = content)
}
