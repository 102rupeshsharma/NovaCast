// JNI bridge between the Android app (Kotlin, dev.novacast.android.engine.NativeBridge)
// and the shared NovaCast engine. Deliberately thin: it converts types, routes
// the engine log to logcat, and keeps C++ exceptions from crossing into Java.
//
// Threading: the Kotlin side calls every function from one dedicated thread,
// so an Emulator instance is never used concurrently.

#include "common/log.h"
#include "cpu/sh4.h"
#include "memory/bus.h"
#include "system/emulator.h"
#include "system/scheduler.h"

#include <android/log.h>
#include <jni.h>

#include <exception>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace {

void logcat_sink(nc::LogLevel level, const char* tag, const char* message, void*) {
    int priority = ANDROID_LOG_INFO;
    switch (level) {
    case nc::LogLevel::Trace:
        priority = ANDROID_LOG_VERBOSE;
        break;
    case nc::LogLevel::Debug:
        priority = ANDROID_LOG_DEBUG;
        break;
    case nc::LogLevel::Info:
        priority = ANDROID_LOG_INFO;
        break;
    case nc::LogLevel::Warn:
        priority = ANDROID_LOG_WARN;
        break;
    case nc::LogLevel::Error:
    case nc::LogLevel::Off:
        priority = ANDROID_LOG_ERROR;
        break;
    }
    __android_log_print(priority, "NovaCast", "%s: %s", tag, message);
}

nc::Emulator* emulator_from(jlong handle) {
    return reinterpret_cast<nc::Emulator*>(handle);
}

// Turns a C++ exception into a Java RuntimeException instead of aborting.
void throw_java(JNIEnv* env, const char* what) {
    jclass cls = env->FindClass("java/lang/RuntimeException");
    if (cls)
        env->ThrowNew(cls, what);
}

template <typename F>
auto guarded(JNIEnv* env, F&& body, decltype(body()) fallback) -> decltype(body()) {
    try {
        return body();
    } catch (const std::exception& e) {
        throw_java(env, e.what());
    } catch (...) {
        throw_java(env, "unknown native error");
    }
    return fallback;
}

jstring to_jstring(JNIEnv* env, const std::string& text) {
    return env->NewStringUTF(text.c_str());
}

} // namespace

extern "C" {

JNIEXPORT jint JNI_OnLoad(JavaVM*, void*) {
    nc::log_set_sink(logcat_sink, nullptr);
    LOG_INFO("jni", "NovaCast native library %s loaded", NOVACAST_VERSION);
    return JNI_VERSION_1_6;
}

JNIEXPORT jstring JNICALL Java_dev_novacast_android_engine_NativeBridge_version(JNIEnv* env, jobject) {
    return to_jstring(env, NOVACAST_VERSION);
}

JNIEXPORT jlong JNICALL Java_dev_novacast_android_engine_NativeBridge_create(JNIEnv* env, jobject) {
    return guarded(
        env,
        [&]() -> jlong {
            auto created = nc::Emulator::create();
            if (!created) {
                throw_java(env, created.error().to_string().c_str());
                return 0;
            }
            return reinterpret_cast<jlong>(std::move(created).value().release());
        },
        jlong{0});
}

JNIEXPORT void JNICALL Java_dev_novacast_android_engine_NativeBridge_destroy(JNIEnv*, jobject, jlong handle) {
    delete emulator_from(handle);
}

// Each entry is "id\tstatus\thardware\tnotes".
JNIEXPORT jobjectArray JNICALL Java_dev_novacast_android_engine_NativeBridge_moduleStatus(JNIEnv* env, jobject,
                                                                                          jlong handle) {
    return guarded(
        env,
        [&]() -> jobjectArray {
            const auto modules = emulator_from(handle)->modules();
            jclass string_class = env->FindClass("java/lang/String");
            jobjectArray result = env->NewObjectArray(static_cast<jsize>(modules.size()), string_class, nullptr);
            for (std::size_t i = 0; i < modules.size(); ++i) {
                const auto& m = modules[i];
                std::string row = std::string(m.id) + '\t' + nc::module_status_name(m.status) + '\t' +
                                  std::string(m.hardware) + '\t' + std::string(m.notes);
                jstring s = to_jstring(env, row);
                env->SetObjectArrayElement(result, static_cast<jsize>(i), s);
                env->DeleteLocalRef(s);
            }
            return result;
        },
        static_cast<jobjectArray>(nullptr));
}

JNIEXPORT jboolean JNICALL Java_dev_novacast_android_engine_NativeBridge_canRunSoftware(JNIEnv*, jobject,
                                                                                        jlong handle) {
    return emulator_from(handle)->can_run_software() ? JNI_TRUE : JNI_FALSE;
}

// Returns null on success, or an error message.
JNIEXPORT jstring JNICALL Java_dev_novacast_android_engine_NativeBridge_loadBios(JNIEnv* env, jobject, jlong handle,
                                                                                 jbyteArray image) {
    return guarded(
        env,
        [&]() -> jstring {
            // Copy out of the Java array so nothing stays pinned if the engine throws.
            const jsize length = env->GetArrayLength(image);
            std::vector<nc::u8> bytes(static_cast<std::size_t>(length));
            env->GetByteArrayRegion(image, 0, length, reinterpret_cast<jbyte*>(bytes.data()));
            auto result = emulator_from(handle)->load_bios(std::span<const nc::u8>(bytes));
            if (!result)
                return to_jstring(env, result.error().to_string());
            return nullptr;
        },
        static_cast<jstring>(nullptr));
}

JNIEXPORT void JNICALL Java_dev_novacast_android_engine_NativeBridge_reset(JNIEnv*, jobject, jlong handle) {
    emulator_from(handle)->reset();
}

// Runs the emulated system for `cycles` SH-4 cycles. Null on success.
JNIEXPORT jstring JNICALL Java_dev_novacast_android_engine_NativeBridge_run(JNIEnv* env, jobject, jlong handle,
                                                                            jlong cycles) {
    return guarded(
        env,
        [&]() -> jstring {
            if (cycles <= 0)
                return to_jstring(env, "cycle count must be positive");
            auto result = emulator_from(handle)->run_for(static_cast<nc::u64>(cycles));
            if (!result)
                return to_jstring(env, result.error().to_string());
            return nullptr;
        },
        static_cast<jstring>(nullptr));
}

// [cycles elapsed, instructions executed, PC, PR, SR, sleeping (0/1), unmapped bus accesses]
JNIEXPORT jlongArray JNICALL Java_dev_novacast_android_engine_NativeBridge_snapshot(JNIEnv* env, jobject,
                                                                                    jlong handle) {
    nc::Emulator& emu = *emulator_from(handle);
    const auto& cpu = emu.cpu();
    const auto& s = cpu.state();
    const jlong values[7] = {
        static_cast<jlong>(emu.scheduler().now()),
        static_cast<jlong>(cpu.instructions_executed()),
        static_cast<jlong>(s.pc),
        static_cast<jlong>(s.pr),
        static_cast<jlong>(s.sr),
        cpu.sleeping() ? 1 : 0,
        static_cast<jlong>(emu.bus().unmapped_access_count()),
    };
    jlongArray result = env->NewLongArray(7);
    if (result)
        env->SetLongArrayRegion(result, 0, 7, values);
    return result;
}

} // extern "C"
