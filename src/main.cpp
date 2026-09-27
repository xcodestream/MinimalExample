// MinimalExample — минимальный пример использования движка CrossRender.
//
// Рисует латинский алфавит, по которому бежит волна яркости, а точка
// свечения следует за курсором мыши: буквы под курсором вспыхивают
// белым и плавно угасают к тёмно-серому вдали от него.
//
// Управление: Esc или Q — выход, F1 — отладочный оверлей движка.

#include "crossrender/Engine.h"
#include "crossrender/Resource.h"
#include "crossrender/scene/Scene.h"
#include "crossrender/gfx/Renderer2D.h"
#include "crossrender/text/Font.h"
#include "crossrender/core/Log.h"
#include "crossrender/core/Math.h"

#include <cmath>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr const char* kAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

// Скорость фоновой волны яркости (циклов в секунду) — лёгкое движение,
// заметное глазу, пока курсор не двигается.
constexpr crossrender::f32 kBaseWaveSpeed = 0.06f;
// Доля фоновой волны в итоговой яркости (остальное добирает свечение у курсора).
constexpr crossrender::f32 kBaseWaveAmount = 0.35f;
// Скорость пульса свечения у курсора (циклов в секунду).
constexpr crossrender::f32 kPulseSpeed = 0.25f;
// Радиус свечения вокруг курсора в долях размера шрифта (сигма гауссианы).
constexpr crossrender::f32 kGlowSigma = 1.6f;
// Скорость следования точки свечения за курсором (1/сек).
constexpr crossrender::f32 kCursorFollow = 10.0f;
// Границы яркости букв: тёмно-серый -> белый.
constexpr crossrender::f32 kMinBrightness = 0.25f;
constexpr crossrender::f32 kMaxBrightness = 1.0f;
// Межбуквенный интервал в долях от размера шрифта.
constexpr crossrender::f32 kLetterSpacing = 0.06f;

// Сцена с переливающимся алфавитом: единственный экран приложения.
class AlphabetScene final : public crossrender::Scene {
public:
    [[nodiscard]] const char* Name() const override { return "alphabet"; }
    [[nodiscard]] const char* Description() const override {
        return "Алфавит: волна яркости следует за курсором мыши";
    }

    // Тёмный фон, на котором волна яркости читается лучше всего.
    [[nodiscard]] crossrender::Color ClearColor() const override {
        return crossrender::Color::FromRGB(0x0E1016);
    }

    void OnEnter(crossrender::SceneContext& ctx) override {
        // Растровый атлас 128 px: при выводе ~90 px остаётся чётким.
        // (SDF-путь движка на части глифов даёт артефакты-«подтёки»
        // ниже базовой линии, поэтому здесь используется битмап.)
        crossrender::FontDesc desc;
        desc.pixelHeight = 128.0f;
        crossrender::Font* bold = ctx.engine->Resources().Font_("fonts/Ubuntu-Bold.ttf", desc);
        if (bold == nullptr || !bold->Valid()) {
            // Движок откатывается на встроенный процедурный шрифт —
            // текст рисуется даже без ассетов.
            ENG_LOGW("alphabet", "шрифт из assets не загрузился, используется встроенный");
            bold = crossrender::FontManager::Get().DefaultFont();
        }
        font_ = bold;

        crossrender::FontDesc hintDesc;
        hintDesc.pixelHeight = 48.0f;
        crossrender::Font* regular = ctx.engine->Resources().Font_("fonts/Ubuntu-Regular.ttf", hintDesc);
        hintFont_ = (regular != nullptr && regular->Valid()) ? regular : font_;

        Layout(ctx.viewport);
        lastViewportW_ = ctx.viewport.w;
        lastViewportH_ = ctx.viewport.h;
        // Точка свечения стартует в центре экрана, пока мышь не двигалась.
        cursor_ = crossrender::Vec2{ctx.viewport.x + ctx.viewport.w * 0.5f,
                                    ctx.viewport.y + ctx.viewport.h * 0.5f};
        cursorActive_ = false;
    }

    void Update(crossrender::SceneContext& ctx, crossrender::f32 dt) override {
        time_ += dt;

        // Пересобираем раскладку, когда изменился вьюпорт (размер окна, DPI).
        const crossrender::Rect vp = ctx.viewport;
        if (std::fabs(vp.w - lastViewportW_) > 0.5f || std::fabs(vp.h - lastViewportH_) > 0.5f) {
            Layout(vp);
            lastViewportW_ = vp.w;
            lastViewportH_ = vp.h;
        }

        // Точка свечения плавно следует за курсором. До первого движения
        // мыши (и в headless-режиме) остаётся в центре экрана.
        const crossrender::Input& input = ctx.engine->GetInput();
        const crossrender::Vec2 mouse = input.MousePos();
        const crossrender::Vec2 delta = input.MouseDelta();
        if (!cursorActive_ && (delta.x != 0.0f || delta.y != 0.0f)) {
            cursorActive_ = true;
        }
        if (cursorActive_) {
            const crossrender::f32 follow = std::fmin(1.0f, dt * kCursorFollow);
            cursor_.x += (mouse.x - cursor_.x) * follow;
            cursor_.y += (mouse.y - cursor_.y) * follow;
        }

        if (input.KeyPressed(crossrender::Key::Escape) || input.KeyPressed(crossrender::Key::Q)) {
            ctx.engine->Quit();
        }
    }

    void Render2D(crossrender::SceneContext& ctx) override {
        if (font_ == nullptr || letters_.empty()) return;
        crossrender::Renderer2D& r2d = *ctx.r2d;
        const crossrender::Rect vp = ctx.viewport;

        // Множитель из опорных единиц раскладки (100 px) в текущий размер.
        const crossrender::f32 k = fontSize_ / kRefSize;
        const crossrender::f32 totalWidth = totalRefWidth_ * k;
        const crossrender::f32 topY = vp.y + vp.h * 0.5f - fontSize_ * 0.6f;

        // Заголовок и подсказка.
        const crossrender::Color dim{0.62f, 0.66f, 0.75f, 1.0f};
        r2d.DrawText(*hintFont_, "MinimalExample", vp.x + vp.w * 0.5f, vp.y + 28.0f,
                     dim, 22.0f, crossrender::TextAlign::Center, crossrender::TextBaseline::Top);
        r2d.DrawText(*hintFont_, "Esc / Q — выход, F1 — оверлей движка",
                     vp.x + vp.w * 0.5f, vp.y + vp.h - 40.0f,
                     dim, 15.0f, crossrender::TextAlign::Center, crossrender::TextBaseline::Top);

        // Сам алфавит: по одному вызову на букву. Яркость складывается из
        // медленной фоновой волны (чтобы картинка жила и без мыши) и
        // гауссова свечения вокруг точки, следующей за курсором.
        constexpr crossrender::f32 kTwoPi = 6.2831853f;
        const int count = static_cast<int>(letters_.size());
        const crossrender::f32 sigma = fontSize_ * kGlowSigma;
        const crossrender::f32 invTwoSigmaSq = 1.0f / (2.0f * sigma * sigma);
        const crossrender::f32 centerY = topY + fontSize_ * 0.4f;
        const crossrender::f32 pulse = 0.8f + 0.2f * std::sin(time_ * kTwoPi * kPulseSpeed);

        crossrender::f32 x = vp.x + (vp.w - totalWidth) * 0.5f;
        for (int i = 0; i < count; ++i) {
            const crossrender::f32 letterCenterX = x + letters_[i].advanceRef * k * 0.5f;
            const crossrender::f32 base =
                0.5f + 0.5f * std::sin((time_ * kBaseWaveSpeed +
                                        static_cast<crossrender::f32>(i) /
                                            static_cast<crossrender::f32>(count)) *
                                       kTwoPi);
            const crossrender::f32 dx = letterCenterX - cursor_.x;
            const crossrender::f32 dy = centerY - cursor_.y;
            const crossrender::f32 glow =
                std::exp(-(dx * dx + dy * dy) * invTwoSigmaSq) * pulse;

            const crossrender::f32 t = std::fmin(1.0f, base * kBaseWaveAmount + glow);
            const crossrender::f32 v =
                kMinBrightness + (kMaxBrightness - kMinBrightness) * t;
            const crossrender::Color color(v, v, v);
            r2d.DrawText(*font_, std::string(1, letters_[i].ch), x, topY, color, fontSize_,
                         crossrender::TextAlign::Left, crossrender::TextBaseline::Top);
            x += letters_[i].advanceRef * k + kLetterSpacing * fontSize_;
        }
    }

private:
    // Буква с шириной глифа в опорных единицах (размер шрифта kRefSize).
    // Промасштабировав на fontSize_/kRefSize, получаем ширину для любого размера.
    struct Letter {
        char ch = 0;
        crossrender::f32 advanceRef = 0.0f;
    };

    static constexpr crossrender::f32 kRefSize = 100.0f;

    // Считает позиции и подбирает размер шрифта так, чтобы строка занимала
    // не больше 90% ширины вьюпорта.
    void Layout(const crossrender::Rect& vp) {
        letters_.clear();
        crossrender::f32 sum = 0.0f;
        for (const char* p = kAlphabet; *p != '\0'; ++p) {
            const crossrender::Glyph* g =
                font_->GetGlyph(static_cast<crossrender::u32>(static_cast<unsigned char>(*p)));
            // advance хранится в пикселях растеризации; переводим в опорные единицы.
            const crossrender::f32 advanceRef =
                (g != nullptr ? g->advance : 60.0f) * (kRefSize / font_->Desc().pixelHeight);
            letters_.push_back(Letter{*p, advanceRef});
            sum += advanceRef;
        }

        const int n = static_cast<int>(letters_.size());
        const crossrender::f32 spacingRef = kLetterSpacing * kRefSize;
        const crossrender::f32 totalRef = sum + spacingRef * static_cast<crossrender::f32>(n - 1);

        const crossrender::f32 byWidth = vp.w * 0.9f / totalRef * kRefSize;
        fontSize_ = std::fmin(byWidth, vp.h * 0.35f);
        totalRefWidth_ = totalRef;
    }

    crossrender::Font* font_ = nullptr;
    crossrender::Font* hintFont_ = nullptr;
    std::vector<Letter> letters_;
    crossrender::Vec2 cursor_{};
    bool cursorActive_ = false;
    crossrender::f32 fontSize_ = 64.0f;
    crossrender::f32 totalRefWidth_ = 0.0f;
    crossrender::f32 time_ = 0.0f;
    crossrender::f32 lastViewportW_ = 0.0f;
    crossrender::f32 lastViewportH_ = 0.0f;
};

}  // namespace

int main(int argc, char** argv) {
    // Опциональный headless-режим для CI и проверок без окна:
    //   MinimalExample --headless --frames 120 --screenshot alphabet.png
    bool headless = false;
    int frames = 120;
    std::string screenshot;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--headless") {
            headless = true;
        } else if (arg == "--frames" && i + 1 < argc) {
            frames = std::atoi(argv[++i]);
        } else if (arg == "--screenshot" && i + 1 < argc) {
            screenshot = argv[++i];
        } else {
            ENG_LOGW("main", "неизвестный аргумент '%s' проигнорирован", argv[i]);
        }
    }

    crossrender::EngineConfig cfg;
    cfg.window.title = "MinimalExample";
    cfg.window.width = 1280;
    cfg.window.height = 720;
    cfg.window.vsync = true;
    cfg.title = "MinimalExample";
    cfg.enable3D = false;    // пример полностью 2D
    cfg.enableAudio = false; // звука в примере нет
    cfg.enableUI = true;     // нужен отладочному оверлею (F1)
    cfg.startScene = "alphabet";
    if (headless) {
        cfg.headless = true;
        cfg.headlessTarget.width = 1280;
        cfg.headlessTarget.height = 720;
    }
#ifdef __EMSCRIPTEN__
    // Ассеты запечены в виртуальную ФС (--preload-file assets@/assets).
    cfg.assetsPath = "/assets";
#endif

    const auto setup = [](crossrender::Engine& engine) {
        engine.Scenes().Register("alphabet", [] { return std::make_unique<AlphabetScene>(); });
        engine.onUpdate = [](crossrender::Engine& e, crossrender::f32 dt) {
            (void)dt;
            if (e.GetInput().KeyPressed(crossrender::Key::F1)) e.ToggleDebugOverlay();
        };
    };

    if (headless) {
        crossrender::Engine engine;
        if (!engine.Init(cfg)) return 1;
        setup(engine);
        if (!engine.Scenes().SetScene(cfg.startScene)) return 1;
        for (int i = 0; i < frames; ++i) engine.Step(1.0f / 60.0f);
        if (!screenshot.empty()) {
            const std::string path = engine.Screenshot(screenshot);
            if (path.empty()) {
                ENG_LOGE("main", "скриншот не сохранился");
            } else {
                ENG_LOGI("main", "скриншот: %s", path.c_str());
            }
        }
        engine.Shutdown();
        return 0;
    }

    return crossrender::RunExample(cfg, setup);
}
