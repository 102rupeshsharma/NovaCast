package dev.novacast.android.engine

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

/**
 * Runs the shared C++ engine through the real JNI bridge on an Android
 * runtime. Mirrors checks from the desktop test suite (tests/system).
 */
@RunWith(AndroidJUnit4::class)
class NativeBridgeTest {
    private var handle = 0L

    @Before
    fun setUp() {
        handle = NativeBridge.create()
        assertNotEquals(0L, handle)
    }

    @After
    fun tearDown() {
        NativeBridge.destroy(handle)
    }

    @Test
    fun versionMatchesRepositoryVersionFile() {
        // Both come from the repository's VERSION file: Gradle reads it for the
        // app, CMake reads it for the native engine.
        assertEquals(dev.novacast.android.BuildConfig.VERSION_NAME, NativeBridge.version())
    }

    @Test
    fun reportsSixModulesWithHonestStatus() {
        val modules = NativeBridge.moduleStatus(handle).map { it.split('\t') }
        assertEquals(6, modules.size)
        val status = modules.associate { it[0] to it[1] }
        assertEquals("partial", status["cpu"])
        assertEquals("partial", status["memory"])
        assertEquals("not implemented", status["gpu"])
        assertEquals("not implemented", status["audio"])
        assertEquals("not implemented", status["disc"])
        assertEquals("not implemented", status["controller"])
        assertFalse(NativeBridge.canRunSoftware(handle))
    }

    @Test
    fun rejectsWrongSizeAndBlankBios() {
        assertNotNull(NativeBridge.loadBios(handle, ByteArray(1024)))
        assertNotNull(NativeBridge.loadBios(handle, ByteArray(BIOS_SIZE))) // all zero
        assertNotNull(NativeBridge.run(handle, 100)) // no BIOS loaded yet
    }

    @Test
    fun bootsSyntheticRomFromResetVector() {
        assertNull(NativeBridge.loadBios(handle, syntheticRom()))
        NativeBridge.reset(handle)
        assertEquals(0xA0000000L, NativeBridge.snapshot(handle)[2])

        assertNull(NativeBridge.run(handle, 1_000))
        val s = NativeBridge.snapshot(handle)
        assertTrue("cycles ${s[0]}", s[0] in 1_000L..1_001L)
        assertEquals("parked in the BRA . loop", 0xA0000006L, s[2])
        assertEquals("no unmapped accesses", 0L, s[6])
    }

    @Test
    fun longRunKeepsTimeConsistent() {
        assertNull(NativeBridge.loadBios(handle, syntheticRom()))
        NativeBridge.reset(handle)
        repeat(50) { assertNull(NativeBridge.run(handle, 2_000_000)) }
        val s = NativeBridge.snapshot(handle)
        assertTrue("cycles ${s[0]}", s[0] in 100_000_000L..100_000_001L)
        assertEquals(s[0], s[1]) // one cycle per instruction
    }

    private companion object {
        const val BIOS_SIZE = 2 * 1024 * 1024

        // Same synthetic program as tests/system/test_emulator.cpp: store 42 to
        // 0x8C000100, then loop forever. Not Sega code.
        fun syntheticRom(): ByteArray {
            val rom = ByteArray(BIOS_SIZE)
            val code = intArrayOf(0xD102, 0xE02A, 0x2102, 0xAFFE, 0x0009, 0x0009, 0x0100, 0x8C00)
            code.forEachIndexed { i, op ->
                rom[i * 2] = (op and 0xFF).toByte()
                rom[i * 2 + 1] = (op shr 8).toByte()
            }
            return rom
        }
    }
}
