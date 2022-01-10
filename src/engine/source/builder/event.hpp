#include <iostream>
#include <string>
#include <algorithm>
#include <chrono>
#include <thread>

#include "rapidjson/document.h"
#include "rapidjson/pointer.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include "rxcpp/rx.hpp"

#define GTEST_COUT std::cerr << "[          ] [ INFO ] "

using Value = rapidjson::Value;

namespace builder
{

    /**
     * @brief Event is an event based on the rapidjson library
     *
     */
    class Event {
    private:
        rapidjson::Document doc;

    public:
        Event(){};
        Event(const char * json) { doc.Parse(json); };
        Event(const Event & e) {
            this->doc.CopyFrom(e.doc, this->doc.GetAllocator());
        };
        Event(const rapidjson::Value & v) { this->doc.CopyFrom(v, this->doc.GetAllocator()); };

        void set(std::string path, const rapidjson::Value & v);
        rapidjson::Value * get(std::string path);
        bool check(std::string path, const rapidjson::Value & expected);
        bool check(std::string path);
        std::string str();
    };

};