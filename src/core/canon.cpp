#include "theos/canon.hpp"
#include <array>
#include <charconv>
#include <span>
#include <vector>

namespace Theos::Scripture {

namespace {

struct Book {
    std::string_view name;
    uint16_t chapters;
};

// Books shared by every Christian canon, in Protestant order, with Protestant chapter counts
constexpr std::array<Book, 39> old_testament = {{
    {"Genesis", 50}, {"Exodus", 40}, {"Leviticus", 27}, {"Numbers", 36}, {"Deuteronomy", 34},
    {"Joshua", 24}, {"Judges", 21}, {"Ruth", 4}, {"1 Samuel", 31}, {"2 Samuel", 24},
    {"1 Kings", 22}, {"2 Kings", 25}, {"1 Chronicles", 29}, {"2 Chronicles", 36}, {"Ezra", 10},
    {"Nehemiah", 13}, {"Esther", 10}, {"Job", 42}, {"Psalms", 150}, {"Proverbs", 31},
    {"Ecclesiastes", 12}, {"Song of Solomon", 8}, {"Isaiah", 66}, {"Jeremiah", 52}, {"Lamentations", 5},
    {"Ezekiel", 48}, {"Daniel", 12}, {"Hosea", 14}, {"Joel", 3}, {"Amos", 9},
    {"Obadiah", 1}, {"Jonah", 4}, {"Micah", 7}, {"Nahum", 3}, {"Habakkuk", 3},
    {"Zephaniah", 3}, {"Haggai", 2}, {"Zechariah", 14}, {"Malachi", 4},
}};

constexpr std::array<Book, 27> new_testament = {{
    {"Matthew", 28}, {"Mark", 16}, {"Luke", 24}, {"John", 21}, {"Acts", 28},
    {"Romans", 16}, {"1 Corinthians", 16}, {"2 Corinthians", 13}, {"Galatians", 6}, {"Ephesians", 6},
    {"Philippians", 4}, {"Colossians", 4}, {"1 Thessalonians", 5}, {"2 Thessalonians", 3}, {"1 Timothy", 6},
    {"2 Timothy", 4}, {"Titus", 3}, {"Philemon", 1}, {"Hebrews", 13}, {"James", 5},
    {"1 Peter", 5}, {"2 Peter", 3}, {"1 John", 5}, {"2 John", 1}, {"3 John", 1},
    {"Jude", 1}, {"Revelation", 22},
}};

// Ayah counts per surah (6,236 in total)
constexpr std::array<uint16_t, 114> quran_ayahs = {
    7, 286, 200, 176, 120, 165, 206, 75, 129, 109, 123, 111, 43, 52, 99, 128, 111, 110, 98, 135,
    112, 78, 118, 64, 77, 227, 93, 88, 69, 60, 34, 30, 73, 54, 45, 83, 182, 88, 75, 85,
    54, 53, 89, 59, 37, 35, 38, 29, 18, 45, 60, 49, 62, 55, 78, 96, 29, 22, 24, 13,
    14, 11, 11, 18, 12, 12, 30, 52, 52, 44, 28, 28, 20, 56, 40, 31, 50, 40, 46, 42,
    29, 19, 36, 25, 22, 17, 19, 26, 30, 20, 15, 21, 11, 8, 8, 19, 5, 8, 8, 11,
    11, 8, 3, 9, 5, 4, 7, 3, 6, 3, 5, 4, 5, 6,
};

std::vector<Book> build_protestant() {
    std::vector<Book> books(old_testament.begin(), old_testament.end());
    books.insert(books.end(), new_testament.begin(), new_testament.end());
    return books;
}

// Catholic: deuterocanon inserted in Vulgate order; Esther with the Greek additions (16), Daniel 14,
// Joel 4 and Malachi 4 per the common Catholic chapter divisions
std::vector<Book> build_catholic() {
    std::vector<Book> books;
    for (const auto& b : old_testament) {
        if (b.name == "Esther") {
            books.push_back({"Tobit", 14});
            books.push_back({"Judith", 16});
            books.push_back({"Esther", 16});
            books.push_back({"1 Maccabees", 16});
            books.push_back({"2 Maccabees", 15});
        } else if (b.name == "Song of Solomon") {
            books.push_back(b);
            books.push_back({"Wisdom", 19});
            books.push_back({"Sirach", 51});
        } else if (b.name == "Lamentations") {
            books.push_back(b);
            books.push_back({"Baruch", 6});
        } else if (b.name == "Daniel") {
            books.push_back({"Daniel", 14});
        } else if (b.name == "Joel") {
            books.push_back({"Joel", 4});
        } else {
            books.push_back(b);
        }
    }
    books.insert(books.end(), new_testament.begin(), new_testament.end());
    return books;
}

// Orthodox: Catholic canon plus 1 Esdras, Prayer of Manasseh, 3 and 4 Maccabees, and Psalm 151
std::vector<Book> build_orthodox() {
    std::vector<Book> books;
    for (const auto& b : build_catholic()) {
        if (b.name == "Psalms") {
            books.push_back({"Psalms", 151});
            continue;
        }
        books.push_back(b);
        if (b.name == "2 Chronicles") {
            books.push_back({"Prayer of Manasseh", 1});
            books.push_back({"1 Esdras", 9});
        } else if (b.name == "2 Maccabees") {
            books.push_back({"3 Maccabees", 7});
            books.push_back({"4 Maccabees", 18});
        }
    }
    return books;
}

const std::vector<Book>& books(AST::Canon canon) {
    static const std::vector<Book> protestant = build_protestant();
    static const std::vector<Book> catholic = build_catholic();
    static const std::vector<Book> orthodox = build_orthodox();
    static const std::vector<Book> none;
    switch (canon) {
        case AST::Canon::Protestant: return protestant;
        case AST::Canon::Catholic: return catholic;
        case AST::Canon::Orthodox: return orthodox;
        case AST::Canon::Quran: return none;
    }
    return none;
}

// Lowercase, spaces removed: "1 Corinthians" -> "1corinthians"
std::string normalize(std::string_view s) {
    std::string out;
    for (char c : s) {
        if (c == ' ') continue;
        out += (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    }
    return out;
}

constexpr std::array<std::pair<std::string_view, std::string_view>, 8> aliases = {{
    {"psalm", "psalms"}, {"songofsongs", "songofsolomon"}, {"canticles", "songofsolomon"},
    {"qoheleth", "ecclesiastes"}, {"ecclesiasticus", "sirach"}, {"wisdomofsolomon", "wisdom"},
    {"revelations", "revelation"}, {"apocalypse", "revelation"},
}};

std::optional<uint16_t> find_book(AST::Canon canon, std::string_view name) {
    std::string key = normalize(name);
    for (const auto& [alias, target] : aliases) {
        if (key == alias) key = target;
    }
    const auto& list = books(canon);
    for (size_t i = 0; i < list.size(); ++i) {
        if (normalize(list[i].name) == key) return static_cast<uint16_t>(i + 1);
    }
    return std::nullopt;
}

std::optional<uint16_t> to_number(std::string_view s) {
    uint16_t value = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec != std::errc{} || ptr != s.data() + s.size() || s.empty()) return std::nullopt;
    return value;
}

// Parses "C", "C:V", "C:V-W", "C:V-C2:W" into ref (chapter/verse fields only)
bool parse_location(std::string_view text, AST::ScriptureRef& ref) {
    const size_t dash = text.find('-');
    const std::string_view start = text.substr(0, dash);
    const size_t colon = start.find(':');
    auto chapter = to_number(start.substr(0, colon));
    if (!chapter) return false;
    ref.chapter = ref.end_chapter = *chapter;
    if (colon != std::string_view::npos) {
        auto verse = to_number(start.substr(colon + 1));
        if (!verse) return false;
        ref.verse = ref.end_verse = *verse;
    }
    if (dash == std::string_view::npos) return true;
    if (ref.verse == 0) return false;
    const std::string_view end = text.substr(dash + 1);
    const size_t end_colon = end.find(':');
    if (end_colon == std::string_view::npos) {
        auto verse = to_number(end);
        if (!verse) return false;
        ref.end_verse = *verse;
        return true;
    }
    auto end_chapter = to_number(end.substr(0, end_colon));
    auto end_verse = to_number(end.substr(end_colon + 1));
    if (!end_chapter || !end_verse) return false;
    ref.end_chapter = *end_chapter;
    ref.end_verse = *end_verse;
    return true;
}

} // namespace

std::optional<AST::Canon> from_name(std::string_view n) {
    if (n == "protestant") return AST::Canon::Protestant;
    if (n == "catholic") return AST::Canon::Catholic;
    if (n == "orthodox") return AST::Canon::Orthodox;
    if (n == "quran") return AST::Canon::Quran;
    return std::nullopt;
}

std::string_view name(AST::Canon canon) {
    switch (canon) {
        case AST::Canon::Protestant: return "protestant";
        case AST::Canon::Catholic: return "catholic";
        case AST::Canon::Orthodox: return "orthodox";
        case AST::Canon::Quran: return "quran";
    }
    return "";
}

uint16_t book_count(AST::Canon canon) { return static_cast<uint16_t>(books(canon).size()); }

std::string_view book_name(AST::Canon canon, uint16_t book) {
    const auto& list = books(canon);
    return book >= 1 && book <= list.size() ? list[book - 1].name : std::string_view{};
}

std::expected<AST::ScriptureRef, std::string> resolve(AST::Canon canon, std::string_view reference) {
    AST::ScriptureRef ref;
    ref.canon = canon;
    const std::string text(reference);
    const auto bad_form = [&] {
        return std::unexpected("malformed reference '" + text + "'; expected " +
                               (canon == AST::Canon::Quran ? "'S:V' or 'S:V-W'" : "'Book C', 'Book C:V' or 'Book C:V-W'"));
    };

    std::string_view location = reference;
    uint16_t max_chapter = 0;
    if (canon == AST::Canon::Quran) {
        max_chapter = static_cast<uint16_t>(quran_ayahs.size());
    } else {
        const size_t space = reference.rfind(' ');
        if (space == std::string_view::npos) return bad_form();
        const std::string_view book_text = reference.substr(0, space);
        auto book = find_book(canon, book_text);
        if (!book) return std::unexpected("unknown book '" + std::string(book_text) + "' in the " + std::string(name(canon)) + " canon");
        ref.book = *book;
        max_chapter = books(canon)[*book - 1].chapters;
        location = reference.substr(space + 1);
    }

    if (!parse_location(location, ref)) return bad_form();

    const std::string where = canon == AST::Canon::Quran ? "the Quran" : std::string(book_name(canon, ref.book));
    const std::string unit = canon == AST::Canon::Quran ? "surah" : "chapter";
    if (ref.chapter < 1 || ref.chapter > max_chapter || ref.end_chapter < ref.chapter || ref.end_chapter > max_chapter) {
        return std::unexpected(unit + " out of range in '" + text + "': " + where + " has " + std::to_string(max_chapter) + " " + unit + "s");
    }
    if (ref.verse != 0 && (ref.end_verse < 1 || (ref.end_chapter == ref.chapter && ref.end_verse < ref.verse))) {
        return std::unexpected("verse range is empty or reversed in '" + text + "'");
    }
    if (canon == AST::Canon::Quran) {
        if (ref.verse > quran_ayahs[ref.chapter - 1] || ref.end_verse > quran_ayahs[ref.end_chapter - 1]) {
            return std::unexpected("verse out of range in '" + text + "'");
        }
    }
    return ref;
}

std::string format(const AST::ScriptureRef& ref) {
    std::string out = ref.canon == AST::Canon::Quran ? "" : std::string(book_name(ref.canon, ref.book)) + " ";
    out += std::to_string(ref.chapter);
    if (ref.verse == 0) return out;
    out += ":" + std::to_string(ref.verse);
    if (ref.end_chapter != ref.chapter) return out + "-" + std::to_string(ref.end_chapter) + ":" + std::to_string(ref.end_verse);
    if (ref.end_verse != ref.verse) out += "-" + std::to_string(ref.end_verse);
    return out;
}

} // namespace Theos::Scripture
