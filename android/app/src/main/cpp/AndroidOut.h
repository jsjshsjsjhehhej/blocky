#pragma once

#include <android/log.h>
#include <sstream>

// aout << "..." << std::endl;  finisce su logcat (tag "BlockyPreview").
extern std::ostream aout;

class AndroidOut : public std::stringbuf {
public:
    explicit AndroidOut(const char* logTag) : logTag_(logTag) {}

protected:
    int sync() override {
        __android_log_print(ANDROID_LOG_DEBUG, logTag_, "%s", str().c_str());
        str("");
        return 0;
    }

private:
    const char* logTag_;
};
