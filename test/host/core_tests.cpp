#include "Test.h"
#include "MockBoard.h"
#include "Application.h"
#include "game/Game.h"
#include "game/Buttons.h"
#include "game/Display.h"
#include "Config.h"

#include <algorithm>
#include <cstring>

using namespace Simon;

namespace {

void generationRegression()
{
    // Хеши сняты с исходных таблиц/правил ДО миграции.
    // В эталоне random вычислялся независимо через uint64 * 16807 % 2147483647.
    const uint64_t reference[3] = {
        9762114518503023787ULL, 16567786508947270870ULL, 8271581452712533068ULL,
    };
    const uint8_t lengths[3][3] = {{3, 3, 4}, {3, 4, 5}, {4, 5, 6}};

    for (uint8_t level = 0; level < 3; ++level) {
        uint64_t hash = 14695981039346656037ULL;
        auto add = [&](uint8_t value) { hash = (hash ^ value) * 1099511628211ULL; };
        for (uint16_t seed = 0; seed <= 9999; ++seed) {
            Random random(seed);
            for (uint8_t stage = 0; stage < STAGES; ++stage) {
                Sequence sequence;
                CHECK(generateSequence(seed, Level(level), stage, random, sequence));
                CHECK(sequence.length == lengths[level][stage]);
                CHECK(sequence.column < (level == EASY ? 2 : level == MEDIUM ? 4 : 6));
                add(sequence.column); add(sequence.length);
                for (uint8_t i = 0; i < sequence.length; ++i) {
                    CHECK(sequence.colors[i] < COLOR_COUNT);
                    CHECK(sequence.buttons[i] >= 1 && sequence.buttons[i] <= 5);
                    add(sequence.colors[i]); add(sequence.buttons[i]);
                }
            }
        }
        CHECK(hash == reference[level]);
    }
}

void randomAndInvalidArguments()
{
    for (uint16_t seed : {0, 1, 9999, 65535}) {
        Random random(seed);
        uint64_t expected = uint32_t(seed) + 1;
        for (unsigned i = 0; i < 1000; ++i) {
            expected = expected * 16807 % 2147483647;
            CHECK(random.next() == expected);
        }
    }

    Sequence sequence;
    sequence.length = 0xCC;
    Random invalidRandom(42), control(42);
    CHECK(!generateSequence(10000, EASY, 0, invalidRandom, sequence));
    CHECK(!generateSequence(0, Level(3), 0, invalidRandom, sequence));
    CHECK(!generateSequence(0, HARD, 3, invalidRandom, sequence));
    CHECK(sequence.length == 0xCC);
    CHECK(invalidRandom.next() == control.next());
    CHECK(pickColumn(10000, EASY, 0) == 0xFF);
    CHECK(pickColumn(0, Level(255), 0) == 0xFF);
    CHECK(pickColumn(0, HARD, 255) == 0xFF);
    CHECK(buttonFor(Color(255), EASY, 0) == 0);
    CHECK(buttonFor(RED, Level(255), 0) == 0);
    for (uint8_t l = 0; l < 3; ++l) {
        CHECK(buttonFor(RED, Level(l), l == 0 ? 2 : l == 1 ? 4 : 6) == 0);
    }
    CHECK(buttonFor(PINK, EASY, 0) == 0);
    CHECK(pickColumn(0, HARD, 0) == 3); // Повтор важнее нуля.
    CHECK(pickColumn(9999, HARD, 0) == 3); // Повтор важнее суммы > 20.
}

void solveStage(Game &game, uint32_t now)
{
    const Sequence sequence = game.sequence();
    for (uint8_t i = 0; i < sequence.length; ++i) {
        const Event event = game.press(sequence.buttons[i], now);
        CHECK(event == (game.state() == State::Solved ? Event::Solved : Event::None));
    }
}

void gameStages()
{
    for (uint8_t l = 0; l < 3; ++l) {
        Game game;
        CHECK(game.state() == State::Waiting);
        CHECK(game.shownColor() == NO_COLOR);
        CHECK(game.press(1, 0) == Event::None);
        CHECK(game.start(9999, Level(l), 10));
        const Sequence first = game.sequence();
        CHECK(!game.start(10000, EASY, 20));
        CHECK(!game.start(0, Level(255), 20));
        CHECK(game.seed() == 9999 && game.level() == Level(l));

        for (uint8_t stage = 0; stage < STAGES; ++stage) {
            CHECK(game.stage() == stage);
            CHECK(game.state() == State::Playing);
            CHECK(game.press(0, 20) == Event::None);
            CHECK(game.press(6, 20) == Event::None);

            const uint8_t expected = game.sequence().buttons[0];
            CHECK(game.press(expected, 20) == Event::None);
            CHECK(game.inputPosition() == 1);
            const uint8_t wrong = game.sequence().buttons[1] % 5 + 1;
            CHECK(game.press(wrong, 21) == Event::SmallError);
            CHECK(game.inputPosition() == 0 && game.errorActive());
            game.update(21 + Config::ERROR_MS - 1);
            CHECK(game.errorActive());
            game.update(21 + Config::ERROR_MS);
            CHECK(!game.errorActive());

            solveStage(game, 1000);
            CHECK(game.shownColor() == NO_COLOR);
            if (stage < 2) {
                CHECK(game.state() == State::StagePause);
                CHECK(game.press(game.sequence().buttons[0], 1001) == Event::None);
                game.update(1000 + Config::STAGE_DONE_MS - 1);
                CHECK(game.state() == State::StagePause);
                game.update(1000 + Config::STAGE_DONE_MS);
            }
        }
        CHECK(game.state() == State::Solved && game.stage() == 3);
        CHECK(game.press(1, 2000) == Event::None);
        CHECK(game.start(9999, Level(l), 3000));
        CHECK(!memcmp(first.colors, game.sequence().colors, MAX_SEQUENCE));
        solveStage(game, 3000);
        game.stop();
        game.update(10000);
        CHECK(game.state() == State::Waiting && game.shownColor() == NO_COLOR);
    }
}

void showTimingAndWrap()
{
    Game game;
    uint32_t now = UINT32_MAX - 200;
    CHECK(game.start(0, HARD, now));

    for (uint8_t i = 0; i < game.sequence().length; ++i) {
        CHECK(game.shownColor() == game.sequence().colors[i]);
        game.update(now + Config::FLASH_ON_MS - 1);
        CHECK(game.shownColor() != NO_COLOR);
        now += Config::FLASH_ON_MS;
        game.update(now);
        CHECK(game.shownColor() == NO_COLOR);
        game.update(now + Config::FLASH_OFF_MS - 1);
        CHECK(game.shownColor() == NO_COLOR);
        now += Config::FLASH_OFF_MS;
        game.update(now);
    }
    CHECK(game.shownColor() == NO_COLOR);
    game.update(now + Config::SHOW_PAUSE_MS - 1);
    CHECK(game.shownColor() == NO_COLOR);
    game.update(now + Config::SHOW_PAUSE_MS);
    CHECK(game.shownColor() == game.sequence().colors[0]);

    now = UINT32_MAX - 300;
    CHECK(game.start(42, MEDIUM, now));
    CHECK(game.press(game.sequence().buttons[0] % 5 + 1, now) == Event::SmallError);
    game.update(now + Config::ERROR_MS - 1);
    CHECK(game.errorActive());
    game.update(now + Config::ERROR_MS);
    CHECK(!game.errorActive());
    solveStage(game, now);
    game.update(now + Config::STAGE_DONE_MS - 1);
    CHECK(game.state() == State::StagePause);
    game.update(now + Config::STAGE_DONE_MS);
    CHECK(game.state() == State::Playing && game.stage() == 1);
}

void buttons()
{
    Buttons buttons;
    const uint32_t start = UINT32_MAX - 10;
    buttons.reset(0, start);
    CHECK(buttons.update(1, start) == 0);
    CHECK(buttons.update(1, start + 29) == 0);
    CHECK(buttons.update(1, start + 30) == 1);
    CHECK(buttons.update(1, start + 100) == 0);
    CHECK(buttons.update(0, start + 101) == 0);
    CHECK(buttons.update(1, start + 110) == 0); // Отскок отпускания.
    CHECK(buttons.update(1, start + 200) == 0);
    CHECK(buttons.update(0, start + 201) == 0);
    CHECK(buttons.update(0, start + 231) == 0);
    CHECK(buttons.update(0xFF, start + 240) == 0);
    CHECK(buttons.update(0xFF, start + 270) == 0x1F);

    buttons.reset(0x1F, 0);
    CHECK(buttons.update(0x1F, 100) == 0); // Удержание при старте не вводится.
    CHECK(buttons.update(0, 101) == 0);
    CHECK(buttons.update(0, 131) == 0);
    CHECK(buttons.update(0x10, 132) == 0);
    CHECK(buttons.update(0x10, 162) == 0x10);
}

void protocol()
{
    ModuleBus::Frame original;
    original.type = ModuleBus::SIMON;
    original.operation = ModuleBus::GENERATE;
    original.round = UINT64_MAX;
    original.seed = 9999;
    original.difficulty = 2;
    original.errors = UINT16_MAX;
    uint8_t packet[20];
    ModuleBus::encode(original, packet);
    ModuleBus::Frame decoded;
    CHECK(!ModuleBus::decode(nullptr, sizeof(packet), decoded));
    CHECK(ModuleBus::decode(packet, sizeof(packet), decoded));
    CHECK(decoded.round == UINT64_MAX && decoded.seed == 9999);
    CHECK(decoded.errors == UINT16_MAX && decoded.difficulty == 2);

    for (unsigned i = 0; i < 160; ++i) {
        packet[i / 8] ^= 1U << (i % 8);
        decoded.seed = 123;
        CHECK(!ModuleBus::decode(packet, sizeof(packet), decoded));
        CHECK(decoded.seed == 123);
        packet[i / 8] ^= 1U << (i % 8);
    }
    for (size_t size = 0; size < 40; ++size) {
        if (size != 20) {
            CHECK(!ModuleBus::decode(packet, size, decoded));
        }
    }

    auto invalid = [&](ModuleBus::Frame frame) {
        ModuleBus::encode(frame, packet);
        CHECK(!ModuleBus::decode(packet, 20, decoded));
    };
    auto frame = original; frame.seed = 10000; invalid(frame);
    frame = original; frame.round = 0; invalid(frame);
    frame = original; frame.type = 0; invalid(frame);
    frame = original; frame.operation = 4; invalid(frame);
    frame = original; frame.difficulty = 3; invalid(frame);
    frame = original; frame.state = 4; invalid(frame);
}

void node()
{
    Mock::reset();
    Node node;
    node.begin();
    CHECK(Mock::address == 0x11 && Mock::status().state == ModuleBus::WAITING);
    node.smallError(); node.solved();
    CHECK(Mock::status().state == ModuleBus::WAITING);
    Mock::deliver(UINT64_MAX, 0, 2);
    CHECK(node.update() == Action::NewGame);
    CHECK(Mock::status().round == UINT64_MAX && Mock::status().seed == 0);
    node.smallError(); node.smallError();
    Mock::deliver(UINT64_MAX, 9999, 0);
    CHECK(node.update() == Action::None); // Дубликат не меняет даже параметры.
    CHECK(Mock::status().seed == 0 && Mock::status().errors == 2);

    Mock::deliver(1, 42, 0, ModuleBus::GENERATE, ModuleBus::LABYRINTH);
    CHECK(node.update() == Action::None);
    Mock::deliver(1);
    Mock::commands.front()[19] ^= 1;
    CHECK(node.update() == Action::None);
    Mock::deliver(1, 10000);
    CHECK(node.update() == Action::None);
    Mock::deliver(1, 0, 3);
    CHECK(node.update() == Action::None);
    Mock::deliver(0);
    CHECK(node.update() == Action::None);
    Mock::deliver(5, 0, 0, ModuleBus::STOP);
    CHECK(node.update() == Action::None);
    Mock::deliver(UINT64_MAX, 0, 0, ModuleBus::STOP);
    CHECK(node.update() == Action::Halt);
    node.smallError(); node.solved();
    CHECK(Mock::status().state == ModuleBus::STOPPED && Mock::status().errors == 2);
    Mock::deliver(UINT64_MAX);
    CHECK(node.update() == Action::None); // STOP не оживляет повтор GENERATE.

    Mock::deliver(2, 9999, 1);
    CHECK(node.update() == Action::NewGame);
    for (uint32_t i = 0; i < 65537; ++i) { node.smallError(); }
    CHECK(Mock::status().errors == UINT16_MAX);
    node.solved(); node.smallError(); node.solved();
    CHECK(Mock::status().state == ModuleBus::SOLVED && Mock::status().errors == UINT16_MAX);
    Mock::deliver(0, 0, 0, ModuleBus::RESET);
    CHECK(node.update() == Action::Halt);
    CHECK(Mock::status().round == 0 && Mock::status().errors == 0);
    Mock::deliver(2);
    CHECK(node.update() == Action::NewGame);
    CHECK(node.update() == Action::None);
}

void display()
{
    Mock::reset();
    Display display;
    Game game;
    display.update(game, 0);
    CHECK(Mock::digits == std::vector<uint8_t>({0, 0, 0, 0}));
    CHECK(Mock::rgb == std::vector<uint8_t>({0, 0, 0}));
    CHECK(Mock::level.size() == 6 && Mock::progress.size() == 12);
    display.update(game, 1);
    CHECK(Mock::digitCalls == 1 && Mock::levelCalls == 1 && Mock::progressCalls == 1);

    CHECK(game.start(42, EASY, 100));
    display.update(game, 100);
    CHECK(Mock::digits == std::vector<uint8_t>({0x3F, 0x3F, 0x66, 0x5B}));
    CHECK(Mock::level == std::vector<uint8_t>({60, 0, 0, 0, 0, 0}));
    CHECK(Mock::progress[0] == 42 && Mock::progress[1] == 60);
    CHECK(Mock::progress[9] == 0 && Mock::progress[10] == 0 && Mock::progress[11] == 0);

    game.press(game.sequence().buttons[0] % 5 + 1, 101);
    display.update(game, 101);
    CHECK(Mock::progress[0] == 0 && Mock::progress[1] == 60);
    solveStage(game, 102);
    display.update(game, 102);
    CHECK(Mock::progress[0] == 60 && Mock::progress[1] == 0);
    CHECK(Mock::rgb == std::vector<uint8_t>({0, 0, 0}));

    Mock::digitsOk = Mock::pixelsOk = false;
    game.start(9999, HARD, 200);
    display.update(game, 200);
    const unsigned calls = Mock::digitCalls;
    display.update(game, 299);
    CHECK(Mock::digitCalls == calls);
    Mock::digitsOk = Mock::pixelsOk = true;
    display.update(game, 300);
    CHECK(Mock::digitCalls == calls + 1 && Mock::level[1] == 60);
    game.stop();
    display.update(game, 400);
    CHECK(Mock::digits == std::vector<uint8_t>({0, 0, 0, 0}));
    CHECK(std::all_of(Mock::progress.begin(), Mock::progress.end(), [](uint8_t b) { return b == 0; }));

    // Новый сид не ждёт интервала повторной попытки предыдущего обновления.
    game.start(1, MEDIUM, 401);
    display.update(game, 401);
    CHECK(Mock::digits == std::vector<uint8_t>({0x3F, 0x3F, 0x3F, 0x06}));
    CHECK(Mock::level[0] == 42 && Mock::level[1] == 60);
}

void integration()
{
    Mock::reset();
    Application app;
    app.begin();
    app.update();
    CHECK(Mock::status().state == ModuleBus::WAITING);
    Mock::deliver(100, 42, 2);
    app.update();
    CHECK(Mock::status().state == ModuleBus::PLAYING);
    Game reference;
    CHECK(reference.start(42, HARD, Mock::now));

    auto press = [&](uint8_t button) {
        Mock::buttons = 1U << (button - 1);
        app.update();
        Mock::now += Config::DEBOUNCE_MS - 1;
        app.update();
        Mock::now += 1;
        app.update();
        Mock::now += 10;
        app.update(); // Удержание не повторяется.
        Mock::buttons = 0;
        app.update();
        Mock::now += Config::DEBOUNCE_MS;
        app.update();
    };

    press(reference.sequence().buttons[0] % 5 + 1);
    CHECK(Mock::status().errors == 1);
    Mock::deliver(100, 9999, 0);
    app.update();
    CHECK(Mock::status().seed == 42 && Mock::status().difficulty == 2);
    CHECK(Mock::status().errors == 1);

    for (uint8_t stage = 0; stage < 3; ++stage) {
        const Sequence sequence = reference.sequence();
        for (uint8_t i = 0; i < sequence.length; ++i) {
            press(sequence.buttons[i]);
        }
        solveStage(reference, Mock::now);
        if (stage < 2) {
            Mock::now += Config::STAGE_DONE_MS;
            app.update();
            reference.update(Mock::now);
        }
    }
    CHECK(Mock::status().state == ModuleBus::SOLVED);
    CHECK(Mock::status().errors == 1);
    CHECK(Mock::progress[0] == 60 && Mock::progress[3] == 60 && Mock::progress[6] == 60);

    Mock::deliver(101);
    app.update();
    Mock::deliver(101, 0, 0, ModuleBus::STOP);
    app.update();
    CHECK(Mock::status().state == ModuleBus::STOPPED);
    CHECK(Mock::rgb == std::vector<uint8_t>({0, 0, 0}));
    Mock::deliver(0, 0, 0, ModuleBus::RESET);
    app.update();
    CHECK(Mock::status().state == ModuleBus::WAITING && Mock::status().round == 0);
}

} // namespace

int main()
{
    try {
        run("all 30000 seed/level combinations match original firmware", generationRegression);
        run("random stream and invalid argument boundaries", randomAndInvalidArguments);
        run("all stages, errors, stop and deterministic restart", gameStages);
        run("show timing, exact deadlines and uint32 wrap", showTimingAndWrap);
        run("button debounce, bounce, hold, simultaneous and wrap", buttons);
        run("protocol CRC every bit and invalid fields", protocol);
        run("node duplicates, counters, stop/reset and validation", node);
        run("display colors, leading zeros, dirty retry and halt", display);
        run("application with gateway commands and debounced presses", integration);
        std::cout << cases << " groups, " << checks << " checks\n";
    } catch (const std::exception &error) {
        std::cerr << "FAIL " << error.what() << "\n";
        return 1;
    }
}
