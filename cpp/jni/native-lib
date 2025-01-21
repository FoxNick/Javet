#include "native-lib.h"
#include <string>
#include <thread>
#include <node.h>
#include <unistd.h>

/**
 * Redirect stdout and staderr to Android's logcat
 */
// 全局变量存储 Java OutputCallback 对象
JavaVM *gJavaVM = nullptr;
jobject gJavaObj = nullptr;


extern "C"
JNIEXPORT void JNICALL
Java_com_node_android_NodeLoader_setOutputCallback(JNIEnv *env, jobject, jobject callback) {
    env->GetJavaVM(&gJavaVM);
    gJavaObj = env->NewGlobalRef(callback);

    nodeonandroid::redirectStreamsToPipe();
    nodeonandroid::startLoggingFromPipe();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_node_android_NodeLoader_startNodeWithArguments(JNIEnv *env, jobject, jobjectArray args) {
    //setenv("NODE_PATH", path_path, 1);
    auto continuousArray = nodeonandroid::makeContinuousArray(env, args);
    auto argv = nodeonandroid::getArgv(continuousArray);

    node::Start(argv.size() - 1, argv.data());
}

namespace nodeonandroid {
    namespace {
        std::thread logger;
        static int pfd[2];
    }

    std::vector<char> makeContinuousArray(JNIEnv *env, jobjectArray fromArgs) {
        int count = env->GetArrayLength(fromArgs);
        std::vector<char> buffer;
        for (int i = 0; i < count; i++) {
            jstring str = (jstring)env->GetObjectArrayElement(fromArgs, i);
            const char* sptr = env->GetStringUTFChars(str, 0);

            do {
                buffer.push_back(*sptr);
            }
            while(*sptr++ != '\0');
        }

        return buffer;
    }

    std::vector<char*> getArgv(std::vector<char>& fromContinuousArray) {
        std::vector<char*> argv;

        argv.push_back(fromContinuousArray.data());
        for (int i = 0; i < fromContinuousArray.size() - 1; i++) {
            if (fromContinuousArray[i] == '\0') argv.push_back(&fromContinuousArray[i+1]);
        }

        argv.push_back(nullptr);

        return argv;
    }

    void redirectStreamsToPipe() {
        setvbuf(stdout, 0, _IOLBF, 0);
        setvbuf(stderr, 0, _IONBF, 0);

        pipe(pfd);
        dup2(pfd[1], 1);
        dup2(pfd[1], 2);
    }

    void startLoggingFromPipe() {
        logger = std::thread([](int *pipefd) {
            char buf[128];
            std::size_t nBytes = 0;
            JNIEnv *env = nullptr;
            gJavaVM->AttachCurrentThread(&env, nullptr);
            jclass thiz = env->GetObjectClass(gJavaObj);
            jmethodID nativeCallback = env->GetMethodID(thiz, "onOutput", "(Ljava/lang/String;)V");
            while ((nBytes = read(pfd[0], buf, sizeof buf - 1)) > 0) {
                if (buf[nBytes - 1] == '\n') --nBytes;
                buf[nBytes] = 0;
                env->CallVoidMethod(gJavaObj,nativeCallback, static_cast<jstring>(env->NewStringUTF(buf)));
                LOGD("%s", buf);
            }
        }, pfd);

        logger.detach();
    }
}

