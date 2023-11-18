//
// Created by jakub on 06.11.2023.
//

#ifndef SAMPLES_ONNXHELPER_H
#define SAMPLES_ONNXHELPER_H
#include <vector>
#include <onnxruntime_cxx_api.h>
#include <onnxruntime_c_api.h> // [jlasecki]: It is actually defined so don't worry
#include <algorithm>
#import <string>

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#ifndef ORT_API_VERSION
#define ORT_API_VERSION   17
#endif

#define LOG_VERBOSE(message, ...) ((void)__android_log_print(ANDROID_LOG_VERBOSE, "MY_LOG_TAG", "[%s %s] " message, __FILE_NAME__, __func__, ##__VA_ARGS__))

// Inspired by this one: https://github.com/microsoft/onnxruntime-inference-examples/blob/main/c_cxx/Snpe_EP/main.cpp
class OnnxHelper {
private:
    bool _checkStatus(OrtStatus* status) {
        if (status != nullptr) {
            const char* msg = g_ort->GetErrorMessage(status);
            std::cerr << msg << std::endl;
            this->g_ort->ReleaseStatus(status);
            throw Ort::Exception(msg, OrtErrorCode::ORT_EP_FAIL);
        }
        return true;
    }

    OrtEnv* env;
    const OrtApi* g_ort;
    OrtSessionOptions* session_options;
    OrtSession* session;
    OrtAllocator* allocator;
    size_t num_input_nodes;
    AAssetManager** mgr;
    AAsset* modelAsset;
    const void * modelDataBuffer;
public:
    OnnxHelper(AAssetManager* manager) {
        this->mgr = &manager;

        this->g_ort = OrtGetApiBase()->GetApi(ORT_API_VERSION);
        this->_checkStatus(this->g_ort->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "test", &this->env));
        this->_checkStatus(this->g_ort->CreateSessionOptions(&this->session_options));
        this->_checkStatus(this->g_ort->SetIntraOpNumThreads(this->session_options, 1));
        this->_checkStatus(this->g_ort->SetSessionGraphOptimizationLevel(this->session_options, ORT_ENABLE_BASIC));

        const char* model_path = "model.onnx";
        this->modelAsset = AAssetManager_open(*this->mgr, model_path, AASSET_MODE_BUFFER);

        std::vector<const char*> options_keys = {"runtime"};
        std::vector<const char*> options_values = {"CPU"};

//        this->_checkStatus(this->g_ort->SessionOptionsAppendExecutionProvider(this->session_options, "SNPE", options_keys.data(),
//                                                     options_values.data(), options_keys.size()));

        size_t modelDataBufferLength = (size_t) AAsset_getLength(this->modelAsset);
        this->modelDataBuffer = AAsset_getBuffer(this->modelAsset);
        this->_checkStatus(this->g_ort->CreateSessionFromArray(this->env,
                                                               this->modelDataBuffer,
                                                               modelDataBufferLength,
                                                               this->session_options,
                                                               &this->session));

//        this->_checkStatus(this->g_ort->CreateSession(this->env, model_path, this->session_options, &this->session));

        this->_checkStatus(this->g_ort->GetAllocatorWithDefaultOptions(&this->allocator));
        this->_checkStatus(this->g_ort->SessionGetInputCount(session, &this->num_input_nodes));
    }

    ~OnnxHelper() {
        AAsset_close(this->modelAsset);
        this->g_ort->ReleaseSession(this->session);
        this->g_ort->ReleaseSessionOptions(this->session_options);
//        delete this->env;
//        delete this->g_ort;
//        delete this->session_options;
//        delete this->session;
//        delete this->allocator;
    }

    float* dumbProcessing(float* input) {
        return input;
    }

    void simpleModelProcessing(float* input, float* output) {
        const int64_t shape[] = {1296};
        size_t dataLenBytes = shape[0] * sizeof(float);
        size_t shapeLen = 1;

        LOG_VERBOSE("1");

        OrtMemoryInfo * memoryInfo = NULL;
        LOG_VERBOSE("1.5");
        this->_checkStatus(this->g_ort->CreateCpuMemoryInfo(OrtDeviceAllocator, OrtMemTypeDefault, &memoryInfo));

        LOG_VERBOSE("2");
        OrtValue * inputTensor = NULL;

        this->_checkStatus(this->g_ort->CreateTensorWithDataAsOrtValue(memoryInfo,
                                                                       input,
                                                                       dataLenBytes,
                                                                       shape,
                                                                       shapeLen,
                                                                       ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT ,
                                                                       &inputTensor));
        LOG_VERBOSE("3");
        OrtValue * outputTensor = NULL;
        this->_checkStatus(this->g_ort->CreateTensorAsOrtValue(this->allocator,
                                                               shape,
                                                               shapeLen,
                                                               ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
                                                               &outputTensor));
        LOG_VERBOSE("4");

        this->g_ort->ReleaseMemoryInfo(memoryInfo);
        LOG_VERBOSE("5");

        size_t inputCount = 0;
        size_t outputCount = 0;
        this->_checkStatus(this->g_ort->SessionGetInputCount(this->session, &inputCount));
        this->_checkStatus(this->g_ort->SessionGetOutputCount(this->session, &outputCount));

        LOG_VERBOSE("5.5");
        char * inputNames[1] = {};
        char * outputNames[1] = {};

        for (size_t i = 0; i < inputCount; i++) {
            char * name;
            this->_checkStatus(this->g_ort->SessionGetInputName(this->session,
                                                                i,
                                                                this->allocator,
                                                                &name));
            inputNames[i] = name;
        }

        for (size_t i = 0; i < outputCount; i++) {
            char * name;
            this->_checkStatus(this->g_ort->SessionGetOutputName(this->session,
                                                                i,
                                                                this->allocator,
                                                                &name));
            outputNames[i] = name;
        }

        LOG_VERBOSE("6");
        this->_checkStatus(this->g_ort->Run(this->session,
                                            nullptr,
                                            inputNames,
                                            &inputTensor,
                                            inputCount,
                                            outputNames,
                                            outputCount,
                                            &outputTensor));

        void * buffer = NULL;
        LOG_VERBOSE("6.5");
        this->_checkStatus(this->g_ort->GetTensorMutableData(outputTensor, &buffer));
        LOG_VERBOSE("7");
        float * floatBuffer = (float *) buffer;
        LOG_VERBOSE("7.5");
        std::copy(floatBuffer, floatBuffer + shape[0], output);
        LOG_VERBOSE("8");
        this->g_ort->ReleaseValue(inputTensor);
        this->g_ort->ReleaseValue(outputTensor);
        LOG_VERBOSE("9");
    }
};

#endif //SAMPLES_ONNXHELPER_H
