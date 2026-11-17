// The fake engine: plays back an engine run saved in a file, so the
// window can be built and tested without root (docs/design.md § How it is
// tested, Recorded runs). Started as the engine is, through
// DEFRAGR_ENGINE: COMMAND [ARGUMENTS].
//
//   DEFRAGR_RECORDING       a folder in tests/fixtures/ holding
//                           COMMAND.bin, the engine's output for COMMAND.
//                           The arguments are not read.
//   DEFRAGR_REPLAY_SECONDS  when above 0, the records are spread evenly
//                           over that many seconds; otherwise the whole
//                           stream is written at once.
//
// Writes the file's bytes unchanged, one record at a time
// (docs/specs/DFR-0002-engine-protocol.md § 4.1), so a recording of a
// broken stream replays broken. Exits as the engine does, with FINISHED's
// result (§ 4.3); with 0 when the recording has no FINISHED; and with 3,
// having written nothing, when there is no recording for COMMAND, so the
// window reads it as a broken engine, as the other fakes do.
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr std::size_t kHeaderBytes = 8;
constexpr std::uint16_t kTypeFinished = 0x0002;

std::uint32_t littleEndian(const std::vector<char> &bytes, std::size_t at, int width)
{
    std::uint32_t value = 0;
    for (int i = width - 1; i >= 0; --i)
        value = value << 8 | static_cast<unsigned char>(bytes[at + i]);
    return value;
}

// Where each record starts, and the file's end. A header cut short, or a
// length running past the end, makes the rest of the file one piece.
std::vector<std::size_t> recordStarts(const std::vector<char> &stream)
{
    std::vector<std::size_t> starts;
    std::size_t at = 0;
    while (at < stream.size()) {
        starts.push_back(at);
        if (stream.size() - at < kHeaderBytes)
            break;
        at += kHeaderBytes + littleEndian(stream, at + 4, 4);
    }
    starts.push_back(stream.size());
    return starts;
}

bool writeAll(const char *bytes, std::size_t count)
{
    while (count > 0) {
        const ssize_t written = ::write(STDOUT_FILENO, bytes, count);
        if (written < 0 && errno == EINTR)
            continue;
        if (written <= 0)
            return false;
        bytes += written;
        count -= static_cast<std::size_t>(written);
    }
    return true;
}

}  // namespace

int main(int argc, char **argv)
{
    const char *folder = std::getenv("DEFRAGR_RECORDING");
    if (argc < 2 || folder == nullptr)
        return 3;
    std::ifstream file(std::string(folder) + '/' + argv[1] + ".bin", std::ios::binary);
    if (!file)
        return 3;
    const std::vector<char> stream{std::istreambuf_iterator<char>(file), {}};

    const char *secondsText = std::getenv("DEFRAGR_REPLAY_SECONDS");
    const double seconds = secondsText ? std::strtod(secondsText, nullptr) : 0;
    const std::vector<std::size_t> starts = recordStarts(stream);
    const std::size_t records = starts.size() - 1;
    const auto gap = std::chrono::duration<double>(seconds > 0 ? seconds / records : 0);

    int result = 0;
    const auto began = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < records; ++i) {
        // Each record keeps to its own slot, so time spent writing does not
        // add up over a long recording.
        std::this_thread::sleep_until(
            began + std::chrono::duration_cast<std::chrono::steady_clock::duration>(gap * i));
        const std::size_t at = starts[i], size = starts[i + 1] - at;
        if (!writeAll(stream.data() + at, size))
            return 1;
        if (size >= kHeaderBytes + 4 && littleEndian(stream, at, 2) == kTypeFinished)
            result = static_cast<std::int32_t>(littleEndian(stream, at + kHeaderBytes, 4));
    }
    return result;
}
