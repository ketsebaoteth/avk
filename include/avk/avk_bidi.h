#pragma once

#include <SheenBidi/SheenBidi.h>
#include <cstdint>
#include <string_view>
#include <vector>

namespace avk {

/**
 * @brief Directional classification for text runs.
 */
enum class TextDirection : uint8_t { LeftToRight = 0, RightToLeft };

/**
 * @brief Individual directional text run produced by Bidi visual reordering.
 */
struct BidiRun {
  size_t offset{0}; // Byte offset in original UTF-8 string
  size_t length{0}; // Byte length of run
  uint8_t level{0}; // Bidi embedding level (even = LTR, odd = RTL)
  TextDirection direction{TextDirection::LeftToRight};
};

/**
 * @brief High-performance Bidirectional (Bidi) text engine powered by
 * SheenBidi. Includes an ultra-fast path for LTR ASCII text to ensure 0 CPU
 * overhead for standard UI labels.
 */
class BidiEngine {
public:
  /**
   * @brief Processes UTF-8 text and returns visually-ordered BidiRuns.
   */
  static std::vector<BidiRun> ProcessText(std::string_view text) {
    std::vector<BidiRun> runs;
    if (text.empty()) {
      return runs;
    }

    /**
     * @brief 1. ULTRA-FAST PATH: Check if text is pure LTR ASCII.
     * Bypasses SheenBidi allocations completely for 95%+ of standard UI
     * strings.
     */
    if (isPureLtrAscii(text)) {
      runs.reserve(1);
      runs.push_back(BidiRun{.offset = 0,
                             .length = text.size(),
                             .level = 0,
                             .direction = TextDirection::LeftToRight});
      return runs;
    }

    /**
     * @brief 2. Full SheenBidi Algorithm Execution for complex/RTL text.
     */
    SBCodepointSequence sequence = {
        SBStringEncodingUTF8,
        const_cast<void *>(static_cast<const void *>(text.data())),
        static_cast<SBUInteger>(text.size())};

    SBAlgorithmRef algorithm = SBAlgorithmCreate(&sequence);
    if (!algorithm) {
      // Fallback to LTR single run on algorithm creation failure
      runs.push_back(BidiRun{0, text.size(), 0, TextDirection::LeftToRight});
      return runs;
    }

    SBParagraphRef paragraph = SBAlgorithmCreateParagraph(
        algorithm, 0, static_cast<SBUInteger>(text.size()), SBLevelDefaultLTR);

    if (paragraph) {
      SBUInteger paragraphLength = SBParagraphGetLength(paragraph);
      SBLineRef line = SBParagraphCreateLine(paragraph, 0, paragraphLength);

      if (line) {
        SBUInteger runCount = SBLineGetRunCount(line);
        const SBRun *runArray = SBLineGetRunsPtr(line);

        if (runCount > 0 && runArray != nullptr) {
          runs.reserve(runCount);
          for (SBUInteger i = 0; i < runCount; ++i) {
            const SBRun &run = runArray[i];
            runs.push_back(BidiRun{
                .offset = static_cast<size_t>(run.offset),
                .length = static_cast<size_t>(run.length),
                .level = static_cast<uint8_t>(run.level),
                .direction = (run.level & 1) ? TextDirection::RightToLeft
                                             : TextDirection::LeftToRight});
          }
        }
        SBLineRelease(line);
      }
      SBParagraphRelease(paragraph);
    }
    SBAlgorithmRelease(algorithm);

    return runs;
  }

private:
  /**
   * @brief Fast inspection helper determining if text contains non-ASCII or RTL
   * codepoints.
   */
  [[nodiscard]] static bool isPureLtrAscii(std::string_view text) noexcept {
    for (unsigned char c : text) {
      // If character is outside ASCII range (0x00 - 0x7F), trigger full
      // SheenBidi analysis
      if (c >= 0x80) {
        return false;
      }
    }
    return true;
  }
};

} // namespace avk
