package dev.novacast.android.engine

/**
 * JNI entry points into the shared NovaCast C++ engine (see
 * app/src/main/cpp/novacast_jni.cpp). Every call must come from
 * [EmulatorEngine]'s single engine thread.
 */
internal object NativeBridge {
    init {
        System.loadLibrary("novacast_jni")
    }

    external fun version(): String
    external fun create(): Long
    external fun destroy(handle: Long)

    /** One "id\tstatus\thardware\tnotes" entry per hardware module. */
    external fun moduleStatus(handle: Long): Array<String>
    external fun canRunSoftware(handle: Long): Boolean

    /** Returns null on success, otherwise the engine's error message. */
    external fun loadBios(handle: Long, image: ByteArray): String?
    external fun reset(handle: Long)

    /** Runs the emulated system for [cycles] SH-4 cycles. Null on success. */
    external fun run(handle: Long, cycles: Long): String?

    /** [cycles, instructions, PC, PR, SR, sleeping, unmapped accesses] */
    external fun snapshot(handle: Long): LongArray
}
