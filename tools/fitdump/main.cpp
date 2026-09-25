// fitdump: print a summary (default) or every decoded message (--raw) of a
// FIT file. Handy for checking the decoder without starting the GUI.

#include <cstdlib>
#include <iostream>
#include <map>
#include <string_view>

#include "fit/activity.hpp"
#include "fit/format.hpp"

namespace {

void printRaw(const fit::DecodedFile& file) {
    for (const auto& m : file.messages) {
        std::cout << "mesg " << m.globalNumber << ':';
        for (const auto& f : m.fields) {
            std::cout << ' ' << int{f.number} << '=';
            if (const auto s = f.asString()) {
                std::cout << '"' << *s << '"';
            } else if (const auto i = f.asInt()) {
                std::cout << *i;
            } else if (const auto v = f.asFloat()) {
                std::cout << *v;
            } else {
                std::cout << '-';
            }
        }
        std::cout << '\n';
    }
}

void printSummary(const fit::DecodedFile& file) {
    std::map<std::uint16_t, int> counts;
    for (const auto& m : file.messages) ++counts[m.globalNumber];

    std::cout << "protocol " << (file.header.protocolVersion >> 4) << '.'
              << (file.header.protocolVersion & 0x0F) << ", profile "
              << file.header.profileVersion / 100.0 << ", " << file.messages.size()
              << " messages\n";
    for (const auto& [global, count] : counts) {
        std::cout << "  mesg " << global << ": " << count << '\n';
    }

    const auto activity = fit::toActivity(file);
    for (const auto& s : activity.sessions) {
        std::cout << "session: " << (s.sport ? fit::sportName(*s.sport) : "?");
        if (s.startTime) std::cout << ", start " << fit::formatUtc(*s.startTime);
        if (s.totalTimerS) std::cout << ", time " << fit::formatDuration(*s.totalTimerS);
        if (s.totalDistanceM) std::cout << ", " << *s.totalDistanceM / 1000.0 << " km";
        if (s.avgHeartRate) std::cout << ", avg HR " << *s.avgHeartRate;
        std::cout << '\n';
    }
    std::cout << "track points: " << activity.points.size() << '\n';
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: fitdump <file.fit> [--raw]\n";
        return EXIT_FAILURE;
    }
    const bool raw = argc > 2 && std::string_view{argv[2]} == "--raw";
    try {
        const auto file = fit::decodeFile(argv[1]);
        raw ? printRaw(file) : printSummary(file);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
