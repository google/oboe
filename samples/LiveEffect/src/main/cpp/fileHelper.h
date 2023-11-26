//
// Created by jakub on 20.11.2023.
//

#ifndef SAMPLES_FILEHELPER_H
#define SAMPLES_FILEHELPER_H

#ifndef ALOG
#define  ALOG(...)  __android_log_print(ANDROID_LOG_INFO,"test",__VA_ARGS__)
#endif


#include <string>
#include <fstream>

class FileHelper {
private:
public:
    FileHelper() {};
    ~FileHelper() {};

    void saveValue(double input, std::string path) {
        std::string msg = std::to_string(input);
        std::fstream file;
        file.open(path, std::fstream::in | std::fstream::out | std::fstream::app);
        file << msg;
        file.close();
    }
};

#endif //SAMPLES_FILEHELPER_H
