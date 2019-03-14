#include "common/logging/log.h"
#include "native_interface.h"

namespace Log {
extern "C" {
JNICALL void Java_org_citra_1emu_citra_LOG_logEntry(JNIEnv* env, jclass type, jint level,
                                                    jstring file_name, jint line_number,
                                                    jstring function, jstring msg) {
    using CitraJNI::GetJString;
    FmtLogMessage(Class::Frontend, static_cast<Level>(level), GetJString(env, file_name).data(),
                  static_cast<unsigned int>(line_number), GetJString(env, function).data(),
                  GetJString(env, msg).data());
}
}
} // namespace Log
