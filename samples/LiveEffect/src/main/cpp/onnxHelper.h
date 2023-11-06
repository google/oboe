//
// Created by jakub on 06.11.2023.
//

#ifndef SAMPLES_ONNXHELPER_H
#define SAMPLES_ONNXHELPER_H
#include <vector>
#include <onnxruntime_cxx_api.h> // [jlasecki]: It is actually defined so don't worry

#ifndef ORT_API_VERSION
#define ORT_API_VERSION   17
#endif

// [jlasecki]: Inspired (mostly copied) from here: https://github.com/microsoft/onnxruntime-inference-examples/blob/main/c_cxx/Snpe_EP/main.cpp
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
    const char* model_path;
    size_t num_input_nodes;
public:
    OnnxHelper() {
        this->g_ort = OrtGetApiBase()->GetApi(ORT_API_VERSION);
        this->_checkStatus(this->g_ort->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "test", &this->env));
        this->_checkStatus(this->g_ort->CreateSessionOptions(&this->session_options));
        this->_checkStatus(this->g_ort->SetIntraOpNumThreads(this->session_options, 1));
        this->_checkStatus(this->g_ort->SetSessionGraphOptimizationLevel(this->session_options, ORT_ENABLE_BASIC));

        // [jlasecki]: Leaving custom execution provider (other than cpu) for later
        /* std::vector<const char*> options_keys = {"runtime", "buffer_type"};
        std::vector<const char*> options_values = {backend.c_str(), "FLOAT"};  // set to TF8 if use quantized data

        CheckStatus(g_ort, g_ort->SessionOptionsAppendExecutionProvider(session_options, "SNPE", options_keys.data(),
                                                                        options_values.data(), options_keys.size())); */

        this->model_path = "./models/model.onnx"; // [jlasecki]: Hardcoded, maybe refactor later

        this->_checkStatus(this->g_ort->CreateSession(env, this->model_path, session_options, &this->session));
        this->_checkStatus(this->g_ort->GetAllocatorWithDefaultOptions(&this->allocator));

        CheckStatus(this->g_ort->SessionGetInputCount(session, &this->num_input_nodes));
    }

    ~OnnxHelper() {
        delete this->env;
        delete this->g_ort;
        delete this->session_options;
        delete this->session;
        delete this->allocator;
        delete this->model_path;
    }

    float* dumbProcessing(float* input) {
        return input;
    }

    float* modelProcessing(float* input) {
        return input;
    }
};

#endif //SAMPLES_ONNXHELPER_H
