# The JNI bridge is looked up by class and method name from C++.
-keep class dev.novacast.android.engine.NativeBridge {
    native <methods>;
}
