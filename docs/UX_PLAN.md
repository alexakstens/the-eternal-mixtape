# UX Integration Plan — The Eternal Mixtape
> 负责人：Evelyne Li（UX / PluginEditor）
> 上次更新：2026-05-03

---

## 0. 两种 Transport 的区别

理解 UI 结构的关键是区分两套播放控件。两者控制的音频对象**完全独立、互不干扰**。

### 主 Transport（Main Transport）
控制**整个项目的全局播放**——播放源文件、混音整体。位于窗口**底部那一行**。

```
[Settings] [Loop] [<<] [Play] [>>] [AUTO SPLICE] [REGEN] [RAND] [REC]
```

调用的 Processor API：
```cpp
processorRef.play()
processorRef.stop()
processorRef.setTransportPosition(ratio)
processorRef.setLoopEnabled(bool)
processorRef.getTransportPositionSeconds()
processorRef.getTransportTotalLengthSeconds()
```

### Splice Transport（Splice 输出回放）
控制 **SPLICE 操作完成后生成的输出文件**（`splice_output.wav`）的独立播放。
位于窗口**中部 splice 输出波形下方**。

```
[Back] [Play] [Stop] [Forward] [Loop]
```

调用的 Processor API：
```cpp
processorRef.playSpliceOutput()
processorRef.stopSpliceOutput()
processorRef.rewindSpliceOutput()
processorRef.seekSpliceOutput(positionSeconds)
processorRef.setSpliceOutputLoop(bool)
processorRef.isSpliceOutputPlaying()
processorRef.getSpliceOutputLengthSeconds()
```

**一句话**：主 transport = 播放原始输入 / 全局混音；splice transport = 试听 remix 结果。

---

## 0.5. 资源与代码现状盘点（写计划前必须先解决的事）

### 问题 1：当前窗口是竖屏，背景图是横屏 ❌
- `Backgorund.png` ：3334 × 2500（4:3 横屏）
- `PluginEditor` 当前 `setSize(720, 900)`：**竖屏 4:5**
- **决议**：改为横屏 **1024 × 768**（4:3，对齐背景图）

### 问题 2：V1.1 设计稿包含 Backgorund.png 没有的图形元素 ⚠️
V1.1 里的这些元素在 `Backgorund.png` 和 `assets/images/buttons/` 里都没有：
- 两个磁带盘（左右大圆盘）
- 中间磁带传动机构
- AUTO SPLICE / REGENERATE / RANDOMIZE 彩色文字按钮
- REC 红色方块按钮
- DENSITY 滑杆图标
- Settings 齿轮
- 主 transport 的 Loop 小图标

**决议**：先用现有素材实现，缺的素材先用 JUCE `TextButton` + 自定义颜色占位。完成 §6 "缺失素材清单"中的截图任务后再替换。

### 问题 3：Backgorund.png 的留白区和现有控件不匹配 ⚠️
Backgorund.png 只画了 3 个明显留白区：
- 中部大米色矩形 → 给波形用
- 左下小黑色圆角矩形 → 给 SPLICE razor 按钮
- 底部细长黑色椭圆 → 给主 transport

但 `PluginEditor` 还摆了 4 轨标签 + 4 个 gain slider + BPM/DENSITY + skipWarp + splice 进度条 + 状态栏 + 整套 stem 分离面板（4 文本框 + Browse + CUDA 开关 + 4 个 stem 波形 + Inspector 调试按钮）。

**决议**：
- Track A/B/C/D 控件叠在中部米色矩形里（设计就是这么用的）
- BPM/DENSITY/skipWarp 放在 SPLICE 按钮右边
- splice transport + progress + status 放在中部米色和底部 transport 之间
- **Stem 分离面板默认隐藏**（见决议 4）

### 问题 4：Stem 分离面板（Demucs UI）应该放哪？ ✅
**决议**：**只在 Expertise Mode（Cmd / Ctrl 按住）时显示**。
- 默认模式：所有 stem 相关控件 `setVisible(false)`，主界面纯净对齐设计稿
- Expertise 模式：所有 stem 相关控件 `setVisible(true)` 显现在主界面下半部分
- 现有 `updateUIForMode()` 函数已处理 expertise 切换，只需把 stem 控件的可见性也加进去

---

## 1. 现有代码框架总览

### 文件结构
```
source/
├── PluginEditor.h / .cpp     ← UX 主体（Evelyne 负责）
├── PluginProcessor.h / .cpp  ← 后端 API 合约（UX 不改算法）
├── SeparationThread.h        ← Demucs 分离线程（Ryan）
├── SpliceThread.h            ← Splice/Remix 线程（已完整实现）
├── WaveformDisplay.h         ← 可复用波形显示组件
├── AudioConversion.h         ← JUCE Buffer ↔ Eigen 转换工具
└── remixing/
    ├── SongAnalyzer.h        ← C++ BPM/Beat/MFCC/SSM/Novelty 分析
    ├── WarpProcessor.h       ← 相位声码器 tempo+pitch warp（需 FFTW3）
    ├── TimeStretcher.h       ← HPSS 相位声码器 time stretch
    ├── BeatAnalyzer.h        ← 旧名 shim，指向 SongAnalyzer
    ├── beat_tracker.h/.c     ← C 版 beat tracker（需 aubio）
    └── song_analyzer.h/.c    ← C 版 song analyzer（需 aubio）

assets/images/
├── Backgorund.png            ← 横屏背景图（注意拼写错误）
├── eternal mixtape_UI_V1.1.png  ← 视觉参考稿
└── buttons/
    ├── Razor_off/hover/click.svg
    ├── Back_off/hover/on.svg
    ├── Play_off/hover/on.svg
    ├── Stop_off/hover/on.svg
    ├── Forward_off/hover/on.svg
    └── Loop_off/hover/on.svg
```

### BinaryData 变量名（CMake 自动打包 `assets/`）
```cpp
BinaryData::Backgorund_png        // 拼写跟原文件名（bug 跟随）
BinaryData::Backgorund_pngSize
BinaryData::Razor_off_svg / Razor_off_svgSize
BinaryData::Play_off_svg / Play_hover_svg / Play_on_svg
BinaryData::Back_off_svg / Back_hover_svg / Back_on_svg
BinaryData::Stop_off_svg / Stop_hover_svg / Stop_on_svg
BinaryData::Forward_off_svg / Forward_hover_svg / Forward_on_svg
BinaryData::Loop_off_svg / Loop_hover_svg / Loop_on_svg
```

### 按钮状态机（off / hover / on-click 如何映射到 JUCE）

每个按钮 SVG 有 3 个状态图（`_off` / `_hover` / `_on` 或 `_click`），但 JUCE 的
`DrawableButton::setImages()` 接受 8 个状态。我们只用其中 5 个，按钮分**瞬时**和**切换**两类：

#### 第 1 类：瞬时按钮（按下触发，松开恢复）— Razor / Back / Stop / Forward

| 按钮 | normal | over (hover) | down (按下瞬间) | 行为 |
|---|---|---|---|---|
| Razor (SPLICE) | `Razor_off` | `Razor_hover` | `Razor_click` | 瞬时触发 splice |
| Back | `Back_off` | `Back_hover` | `Back_on` | 瞬时跳到开头 |
| Stop | `Stop_off` | `Stop_hover` | `Stop_on` | 瞬时停止 |
| Forward | `Forward_off` | `Forward_hover` | `Forward_on` | 瞬时跳到末尾 |

调用 `applySVGImages()` 传 3 组（off / hover / on）。无需 `setClickingTogglesState`。

#### 第 2 类：切换按钮（点一下激活，再点一下取消）— Play / Loop

| 按钮 | normal | over | down | normalOn (激活态) | overOn (激活+hover) | 行为 |
|---|---|---|---|---|---|---|
| Play | `Play_off` | `Play_hover` | `Play_on` | `Play_on` | `Play_hover` | 切换：播放/停止 |
| Loop | `Loop_off` | `Loop_hover` | `Loop_on` | `Loop_on` | `Loop_hover` | 切换：循环 on/off |

调用 `applySVGImages()` 传 5 组。**必须配** `setClickingTogglesState(true)`。
按钮内部状态用 `getToggleState()` 读取，UI ↔ Processor 双向同步用 `timerCallback` 轮询。

#### 实现模板（已在 splice transport 验证过）

```cpp
// 瞬时按钮
applySVGImages (btn,
                BinaryData::X_off_svg,   BinaryData::X_off_svgSize,
                BinaryData::X_hover_svg, BinaryData::X_hover_svgSize,
                BinaryData::X_on_svg,    BinaryData::X_on_svgSize);
btn.onClick = [this] { /* 触发动作 */ };

// 切换按钮
applySVGImages (btn,
                BinaryData::X_off_svg,   BinaryData::X_off_svgSize,
                BinaryData::X_hover_svg, BinaryData::X_hover_svgSize,
                BinaryData::X_on_svg,    BinaryData::X_on_svgSize,    // down
                BinaryData::X_on_svg,    BinaryData::X_on_svgSize,    // normalOn
                BinaryData::X_hover_svg, BinaryData::X_hover_svgSize); // overOn
btn.setClickingTogglesState (true);
btn.onClick = [this] {
    if (btn.getToggleState()) processorRef.activate();
    else                       processorRef.deactivate();
};
```

#### Splice transport vs 主 transport 的状态分布

| 按钮 | Splice transport（中部，已实现）| 主 transport（底部，待实现）|
|---|---|---|
| Back | 瞬时（`rewindSpliceOutput`）| 瞬时（`stop` + `setTransportPosition(0)`）|
| Play | **切换**（`isSpliceOutputPlaying`）| **切换**（需 `isTransportPlaying()` 同步）|
| Stop | 瞬时 | 瞬时 |
| Forward | 瞬时（`seekSpliceOutput(len)`）| 瞬时（`setTransportPosition(1.0)`）|
| Loop | **切换**（`setSpliceOutputLoop`）| **切换**（`setLoopEnabled`）|

---

## 2. 当前 PluginEditor 已完成的 UI 元素

| 控件 | 类型 | 绑定的 API | 状态 |
|---|---|---|---|
| `runtimeLabel` | Label | `getTransportPositionSeconds()` | ✅ |
| `meterLabel` | Label | `getMasterLevels()` | ✅ |
| `trackLabels[4]` | Label | "A: DRUMS"... | ✅ |
| `trackGainSliders[4]` | Slider | `setTrackGain(i, val)` | ✅ |
| `spliceButton` | DrawableButton | Razor SVG → `startSpliceRemix()` | ✅ |
| `bpmSlider` | Slider 线性 | `setGlobalBPM(val)` | ⚠️ 待改旋钮 |
| `densitySlider` | Slider 线性 | `setSpliceDensity(val)` | ✅ |
| `skipWarpToggle` | ToggleButton | `requestSplice(skipWarp)` | ✅ |
| `spliceOutputWaveform` | WaveformDisplay | splice_output.wav | ✅ |
| `spliceProgressBar` | ProgressBar | `spliceThread.getProgress()` | ✅ |
| `spliceStatusLabel` | Label | `spliceThread.getStatusMessage()` | ✅ |
| `spliceBackBtn` | DrawableButton | Back SVG → `rewindSpliceOutput()` | ✅ |
| `splicePlayBtn` | DrawableButton | Play SVG → `playSpliceOutput()` | ✅ |
| `spliceStopBtn` | DrawableButton | Stop SVG → `stopSpliceOutput()` | ✅ |
| `spliceForwardBtn` | DrawableButton | Forward SVG → `seekSpliceOutput()` | ✅ |
| `spliceLoopBtn` | DrawableButton | Loop SVG → `setSpliceOutputLoop()` | ✅ |
| `settingsButton` | TextButton | 文件夹选择 dialog | ⚠️ 无图标 |
| `loopToggle` | ToggleButton | `setLoopEnabled()` | ⚠️ 无图标 |
| `rewindButton` | TextButton `"<<"` | `stop()` + `setTransportPosition(0)` | ❌ 待换 SVG |
| `playButton` | TextButton `"Play"` | `play()` | ❌ 待换 SVG |
| `ffButton` | TextButton `">>"` | `stop()` ← **bug**，应是 fast-forward | ❌ 待换 SVG |
| `autoSpliceButton` | TextButton | `applyAutoSplice()` ← stub | ❌ 未接逻辑 |
| `regenerateButton` | TextButton | `regenerateMix()` ← stub | ❌ 未接逻辑 |
| `randomizeButton` | TextButton | `randomizeMix()` ← stub | ❌ 未接逻辑 |
| `recButton` | TextButton | `startRecording()` / expertise mode | ✅ |
| Stem 分离面板（10+ 控件）| TextButton/TextEditor/Toggle/Waveform | Demucs 接口 | ✅ 但**永远显示**，需改 |

**Expertise Mode（已实现）**：
- 按住 Cmd（macOS）/ Ctrl（Windows/Linux）进入
- `timerCallback()` 每 100ms 检测 `ModifierKeys::isCommandDown()`
- `updateUIForMode()` 切换 recButton 文字
- 待加：切换 stem 分离面板可见性

---

## 3. 横屏布局规划（1024 × 768）

把 Backgorund.png 当作"皮肤底板"对齐，控件按以下区域分布：

```
┌──────────────────────────────────────────────────────────┐ y=0
│  THE ETERNAL MIXTAPE                                     │
│  [VU meter]                          [Runtime 00:00/00:00] │ y=70
├──────────────────────────────────────────────────────────┤
│                                                          │
│        [ Splice Output Waveform — 跨大半个宽度 ]         │ y=110
│                                                          │
│        [ splice transport: Back Play Stop Fwd Loop ]     │ y=240
│        [ splice progress bar ]                           │ y=290
│        [ splice status label ]                           │ y=315
├══ 米色矩形区（背景图上的留白）═══════════════════════════┤ y=340
│  TRACK A    TRACK B    TRACK C    TRACK D                │
│  [slider]   [slider]   [slider]   [slider]               │ y=400
├══════════════════════════════════════════════════════════┤ y=470
│  [SPLICE]  BPM[knob]  DENSITY[slider]  [Skip Warp]       │ y=520
│                                                          │
├══ 底部 transport ═══════════════════════════════════════┤ y=685
│  BPM   [⚙][Loop][<<][Play][>>]   [AUTO][REGEN][RAND][REC]│ y=720
└──────────────────────────────────────────────────────────┘ y=768
```

**Expertise mode 激活时**，stem 分离面板直接覆盖 y=110~330 区域（splice 区临时让位）：

```
├══ Expertise mode ═══════════════════════════════════════┤ y=110
│  Input:  [path ......] [Browse]                          │
│  Model:  [path ......] [Browse]                          │ y=170
│  Output: [path ......] [Browse]                          │
│  [CUDA] [Separate] [Cancel]  [progress bar]              │ y=230
│  [drums.wav] [bass.wav]                                  │ y=270
│  [other.wav] [vocals.wav]                                │ y=310
└──────────────────────────────────────────────────────────┘
```

---

## 4. 待完成任务

### 任务 A0：窗口改为横屏 1024×768（最高优先级，10 分钟）

**改 PluginEditor.cpp 构造函数末尾**：
```cpp
// 原来：
// setSize (720, 900);

// 改为：
setSize (1024, 768);
```

---

### 任务 A：加载 Backgorund.png（高优先级，30 分钟）

**改动 PluginEditor.cpp**：

1. 构造函数中加载（在 `addAndMakeVisible(inspectButton)` 之前）：
```cpp
uiImage = juce::ImageCache::getFromMemory (BinaryData::Backgorund_png,
                                            BinaryData::Backgorund_pngSize);
```

2. `paint()` 替换：
```cpp
void PluginEditor::paint (juce::Graphics& g)
{
    if (uiImage.isValid())
        g.drawImage (uiImage, getLocalBounds().toFloat(),
                     juce::RectanglePlacement::stretchToFit);
    else
        g.fillAll (juce::Colour (0xff383532)); // fallback 背景色

    // VU meter 绘在左上角
    float level = processorRef.getMasterLevels();
    auto meterRect = juce::Rectangle<int> (10, 38, 110, 14);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (meterRect.toFloat(), 3.0f);
    g.setColour (juce::Colour (0xffd4b896));
    g.fillRoundedRectangle (meterRect.withWidth (
        (int)(meterRect.getWidth() * juce::jmin (1.0f, level))).toFloat(), 3.0f);
}
```

---

### 任务 B：Stem 分离面板纳入 Expertise Mode（高优先级，1 小时）

**目标**：默认隐藏 stem 分离面板，按住 Cmd 才显示。

**改动 PluginEditor.cpp 构造函数末尾**——在所有 `addAndMakeVisible` 之后、`updateUIForMode()` 之前，**先把所有 stem 控件设为不可见**：

```cpp
// stem panel 默认隐藏，仅 expertise mode 显示
auto setStemPanelVisible = [this] (bool v) {
    stemInputLabel.setVisible (v);
    stemModelLabel.setVisible (v);
    stemOutputLabel.setVisible (v);
    stemInputEditor.setVisible (v);
    stemModelEditor.setVisible (v);
    stemOutputEditor.setVisible (v);
    stemInputBrowse.setVisible (v);
    stemModelBrowse.setVisible (v);
    stemOutputBrowse.setVisible (v);
    stemProcessButton.setVisible (v);
    stemCancelButton.setVisible (v);
    stemCudaToggle.setVisible (v);
    stemProgressBar.setVisible (v);
    stemStatusLabel.setVisible (v);
    drumsWaveform.setVisible (v);
    bassWaveform.setVisible (v);
    otherWaveform.setVisible (v);
    vocalsWaveform.setVisible (v);
};
setStemPanelVisible (false);

// 把这个 lambda 存为成员，updateUIForMode 复用
```

**改 PluginEditor.h 增加成员**：
```cpp
std::function<void(bool)> setStemPanelVisible_;
```

把上面的 lambda 改成赋给这个成员变量：
```cpp
setStemPanelVisible_ = [this] (bool v) { /* ...如上 */ };
setStemPanelVisible_ (false);
```

**改 `updateUIForMode()`**：
```cpp
void PluginEditor::updateUIForMode()
{
    if (isExpertiseMode_)
    {
        recButton.setButtonText ("REC (choose file)");
        if (setStemPanelVisible_) setStemPanelVisible_ (true);
    }
    else
    {
        recButton.setButtonText ("REC");
        if (setStemPanelVisible_) setStemPanelVisible_ (false);
    }
    resized(); // 重新布局
}
```

---

### 任务 C：主 Transport 改为 DrawableButton + SVG（高优先级，2 小时）

**目标**：把底部 4 个 TextButton 替换成 DrawableButton + SVG，并修复 `ffButton` 错误绑定 `stop()` 的 bug。

**PluginEditor.h** — 替换：
```cpp
// 删除：
// juce::TextButton rewindButton { "<<" };
// juce::TextButton playButton { "Play" };
// juce::TextButton ffButton { ">>" };
// juce::ToggleButton loopToggle { "Loop" };

// 新增：
juce::DrawableButton mainRewindBtn  { "MainBack",    juce::DrawableButton::ImageFitted };
juce::DrawableButton mainPlayBtn    { "MainPlay",    juce::DrawableButton::ImageFitted };
juce::DrawableButton mainStopBtn    { "MainStop",    juce::DrawableButton::ImageFitted };
juce::DrawableButton mainForwardBtn { "MainForward", juce::DrawableButton::ImageFitted };
juce::DrawableButton mainLoopBtn    { "MainLoop",    juce::DrawableButton::ImageFitted };
```

**PluginEditor.cpp 构造函数** — 替换原 main transport 部分：
```cpp
applySVGImages (mainRewindBtn,
                BinaryData::Back_off_svg,   BinaryData::Back_off_svgSize,
                BinaryData::Back_hover_svg, BinaryData::Back_hover_svgSize,
                BinaryData::Back_on_svg,    BinaryData::Back_on_svgSize);
mainRewindBtn.onClick = [this] {
    processorRef.stop();
    processorRef.setTransportPosition (0.0);
};
addAndMakeVisible (mainRewindBtn);

applySVGImages (mainPlayBtn,
                BinaryData::Play_off_svg,   BinaryData::Play_off_svgSize,
                BinaryData::Play_hover_svg, BinaryData::Play_hover_svgSize,
                BinaryData::Play_on_svg,    BinaryData::Play_on_svgSize,
                BinaryData::Play_on_svg,    BinaryData::Play_on_svgSize,
                BinaryData::Play_hover_svg, BinaryData::Play_hover_svgSize);
mainPlayBtn.setClickingTogglesState (true);
mainPlayBtn.onClick = [this] {
    if (mainPlayBtn.getToggleState())
        processorRef.play();
    else
        processorRef.stop();
};
addAndMakeVisible (mainPlayBtn);

applySVGImages (mainStopBtn,
                BinaryData::Stop_off_svg,   BinaryData::Stop_off_svgSize,
                BinaryData::Stop_hover_svg, BinaryData::Stop_hover_svgSize,
                BinaryData::Stop_on_svg,    BinaryData::Stop_on_svgSize);
mainStopBtn.onClick = [this] {
    processorRef.stop();
    processorRef.setTransportPosition (0.0);
    mainPlayBtn.setToggleState (false, juce::dontSendNotification);
};
addAndMakeVisible (mainStopBtn);

applySVGImages (mainForwardBtn,
                BinaryData::Forward_off_svg,   BinaryData::Forward_off_svgSize,
                BinaryData::Forward_hover_svg, BinaryData::Forward_hover_svgSize,
                BinaryData::Forward_on_svg,    BinaryData::Forward_on_svgSize);
mainForwardBtn.onClick = [this] {
    if (processorRef.getTransportTotalLengthSeconds() > 0.0)
        processorRef.setTransportPosition (1.0); // 跳到末尾（修复了原 stop bug）
};
addAndMakeVisible (mainForwardBtn);

applySVGImages (mainLoopBtn,
                BinaryData::Loop_off_svg,   BinaryData::Loop_off_svgSize,
                BinaryData::Loop_hover_svg, BinaryData::Loop_hover_svgSize,
                BinaryData::Loop_on_svg,    BinaryData::Loop_on_svgSize,
                BinaryData::Loop_on_svg,    BinaryData::Loop_on_svgSize,
                BinaryData::Loop_hover_svg, BinaryData::Loop_hover_svgSize);
mainLoopBtn.setClickingTogglesState (true);
mainLoopBtn.onClick = [this] {
    processorRef.setLoopEnabled (mainLoopBtn.getToggleState());
};
addAndMakeVisible (mainLoopBtn);
```

**PluginProcessor.h 增加**：
```cpp
bool isTransportPlaying() const;
```

**PluginProcessor.cpp 增加**：
```cpp
bool PluginProcessor::isTransportPlaying() const { return isPlaying_; }
```

**timerCallback() 末尾增加**：
```cpp
bool mainIsPlaying = processorRef.isTransportPlaying();
if (mainPlayBtn.getToggleState() != mainIsPlaying)
    mainPlayBtn.setToggleState (mainIsPlaying, juce::dontSendNotification);
```

---

### 任务 D：BPM 滑杆改为旋钮（中优先级，30 分钟）

**改 PluginEditor.cpp 构造函数**：
```cpp
// 原来：
// bpmSlider.setSliderStyle (juce::Slider::LinearHorizontal);
// bpmSlider.setTextBoxStyle (juce::Slider::TextBoxRight, true, 50, 20);

// 改为：
bpmSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
bpmSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 48, 16);
bpmSlider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                juce::MathConstants<float>::pi * 2.8f, true);
```

**resized() 中给旋钮一个正方形空间**（见 §5 完整 resized）。

---

### 任务 E：颜色主题适配（中优先级，30 分钟）

从 Backgorund.png 取色：
- 背景深底：`#383532`
- 字体米色：`#C8B698`（标题、Track 标签、Runtime）
- 高亮橙：`#F07030`（按钮、滑杆 thumb）
- 米色面板：`#A89880`

**构造函数末尾添加**：
```cpp
const juce::Colour textColour   (0xffc8b896);
const juce::Colour accentColour (0xffF07030);

for (int i = 0; i < kNumTracks; ++i)
{
    trackLabels[i].setColour (juce::Label::textColourId, textColour);
    trackGainSliders[i].setColour (juce::Slider::thumbColourId, accentColour);
    trackGainSliders[i].setColour (juce::Slider::trackColourId, juce::Colour (0xff555555));
}
bpmLabel.setColour     (juce::Label::textColourId, textColour);
densityLabel.setColour (juce::Label::textColourId, textColour);
runtimeLabel.setColour (juce::Label::textColourId, textColour);
meterLabel.setColour   (juce::Label::textColourId, textColour);
spliceStatusLabel.setColour (juce::Label::textColourId, textColour);

bpmSlider.setColour     (juce::Slider::thumbColourId,    accentColour);
bpmSlider.setColour     (juce::Slider::rotarySliderFillColourId, accentColour);
densitySlider.setColour (juce::Slider::thumbColourId,    accentColour);

// AUTO SPLICE / REGEN / RAND / REC 占位样式
autoSpliceButton.setColour (juce::TextButton::buttonColourId, accentColour);
recButton.setColour        (juce::TextButton::buttonColourId, juce::Colour (0xffc83c2c));
```

---

### 任务 F：填充 stub 实现（低优先级，30 分钟）

`PluginProcessor.cpp` 把 3 个空函数填上最小实现：
```cpp
void PluginProcessor::applyAutoSplice()
{
    if (lastStemOutputDir_ == juce::File{}) return;
    // 用自动检测 BPM 重跑 splice
    requestSplice (lastStemOutputDir_, 0.0, globalBPM_, false, spliceDensity_);
}

void PluginProcessor::regenerateMix()
{
    if (lastStemOutputDir_ == juce::File{}) return;
    // 相同参数重跑（刷新随机种子）
    requestSplice (lastStemOutputDir_, 0.0, globalBPM_, false, spliceDensity_);
}

void PluginProcessor::randomizeMix()
{
    if (lastStemOutputDir_ == juce::File{}) return;
    spliceDensity_ = juce::Random::getSystemRandom().nextFloat();
    requestSplice (lastStemOutputDir_, 0.0, globalBPM_, false, spliceDensity_);
}
```

---

### 任务 G：横屏 resized() 完整重写（高优先级，1.5 小时）

`PluginEditor.cpp` 替换整个 `resized()`：

```cpp
void PluginEditor::resized()
{
    auto r = getLocalBounds();

    // ── 顶部：标题已画在背景里，留出 VU meter + Runtime 区域 ──
    auto top = r.removeFromTop (90);
    meterLabel.setBounds   (top.removeFromLeft (80).reduced (8, 30));
    runtimeLabel.setBounds (top.removeFromRight (160).reduced (8, 30));

    // ── Splice 输出区（默认显示）/ Stem 面板（expertise）──
    if (isExpertiseMode_)
    {
        // Stem 分离面板覆盖中部区域
        auto stem = r.removeFromTop (240);
        const int labelW = 60, browseW = 70, rowH = 26, gap = 4;

        auto row1 = stem.removeFromTop (rowH);
        stemInputLabel.setBounds  (row1.removeFromLeft (labelW));
        stemInputBrowse.setBounds (row1.removeFromRight (browseW));
        stemInputEditor.setBounds (row1);
        stem.removeFromTop (gap);

        auto row2 = stem.removeFromTop (rowH);
        stemModelLabel.setBounds  (row2.removeFromLeft (labelW));
        stemModelBrowse.setBounds (row2.removeFromRight (browseW));
        stemModelEditor.setBounds (row2);
        stem.removeFromTop (gap);

        auto row3 = stem.removeFromTop (rowH);
        stemOutputLabel.setBounds  (row3.removeFromLeft (labelW));
        stemOutputBrowse.setBounds (row3.removeFromRight (browseW));
        stemOutputEditor.setBounds (row3);
        stem.removeFromTop (gap);

        auto ctlRow = stem.removeFromTop (28);
        stemCudaToggle.setBounds    (ctlRow.removeFromLeft (80));
        ctlRow.removeFromLeft (8);
        stemProcessButton.setBounds (ctlRow.removeFromLeft (90));
        ctlRow.removeFromLeft (4);
        stemCancelButton.setBounds  (ctlRow.removeFromLeft (90));
        ctlRow.removeFromLeft (12);
        stemProgressBar.setBounds   (ctlRow.removeFromLeft (200));
        stem.removeFromTop (gap);

        stemStatusLabel.setBounds (stem.removeFromTop (20));
        stem.removeFromTop (gap);

        // 4 个 stem 波形：2x2 网格
        const int waveH = (stem.getHeight() - gap) / 2;
        const int halfW = (stem.getWidth() - gap) / 2;
        auto top1 = stem.removeFromTop (waveH);
        drumsWaveform.setBounds (top1.removeFromLeft (halfW));
        top1.removeFromLeft (gap);
        bassWaveform.setBounds  (top1);
        stem.removeFromTop (gap);
        auto top2 = stem.removeFromTop (waveH);
        otherWaveform.setBounds (top2.removeFromLeft (halfW));
        top2.removeFromLeft (gap);
        vocalsWaveform.setBounds (top2);
    }
    else
    {
        // 默认模式：splice 输出波形 + 进度
        spliceOutputWaveform.setBounds (r.removeFromTop (110).reduced (40, 4));
        r.removeFromTop (6);

        // splice transport 一行
        auto sptRow = r.removeFromTop (40);
        sptRow = sptRow.withSizeKeepingCentre (5 * 40 + 8, 38);
        const int btnW = 40;
        spliceBackBtn.setBounds    (sptRow.removeFromLeft (btnW));
        splicePlayBtn.setBounds    (sptRow.removeFromLeft (btnW));
        spliceStopBtn.setBounds    (sptRow.removeFromLeft (btnW));
        spliceForwardBtn.setBounds (sptRow.removeFromLeft (btnW));
        sptRow.removeFromLeft (8);
        spliceLoopBtn.setBounds    (sptRow.removeFromLeft (btnW));

        spliceProgressBar.setBounds (r.removeFromTop (16).reduced (40, 0));
        r.removeFromTop (4);
        spliceStatusLabel.setBounds (r.removeFromTop (18).reduced (40, 0));
    }

    // ── 米色面板：Track A/B/C/D 标签 + gain slider ──
    r.removeFromTop (8);
    auto trackBand = r.removeFromTop (90);
    const int colW = trackBand.getWidth() / kNumTracks;
    for (int i = 0; i < kNumTracks; ++i)
    {
        auto col = trackBand.removeFromLeft (colW).reduced (8, 4);
        trackLabels[i].setBounds (col.removeFromTop (22));
        trackGainSliders[i].setBounds (col);
    }

    // ── SPLICE / BPM / DENSITY / Skip Warp 行 ──
    r.removeFromTop (8);
    auto spliceRow = r.removeFromTop (80);
    spliceButton.setBounds  (spliceRow.removeFromLeft (60).reduced (4));
    spliceRow.removeFromLeft (12);
    bpmLabel.setBounds      (spliceRow.removeFromLeft (40).withTrimmedTop (24));
    bpmSlider.setBounds     (spliceRow.removeFromLeft (70));
    spliceRow.removeFromLeft (12);
    densityLabel.setBounds  (spliceRow.removeFromLeft (60).withTrimmedTop (24));
    densitySlider.setBounds (spliceRow.removeFromLeft (160).reduced (0, 28));
    spliceRow.removeFromLeft (16);
    skipWarpToggle.setBounds (spliceRow.removeFromLeft (90).withTrimmedTop (28));

    // ── 底部主 transport ──
    auto bottom = r.removeFromBottom (52).reduced (10, 6);
    settingsButton.setBounds  (bottom.removeFromLeft (60));
    bottom.removeFromLeft (4);
    const int btnW = 36;
    mainLoopBtn.setBounds     (bottom.removeFromLeft (btnW));
    mainRewindBtn.setBounds   (bottom.removeFromLeft (btnW));
    mainStopBtn.setBounds     (bottom.removeFromLeft (btnW));
    mainPlayBtn.setBounds     (bottom.removeFromLeft (btnW));
    mainForwardBtn.setBounds  (bottom.removeFromLeft (btnW));
    bottom.removeFromLeft (16);
    autoSpliceButton.setBounds (bottom.removeFromLeft (100));
    bottom.removeFromLeft (4);
    regenerateButton.setBounds (bottom.removeFromLeft (100));
    bottom.removeFromLeft (4);
    randomizeButton.setBounds  (bottom.removeFromLeft (90));
    bottom.removeFromLeft (4);
    recButton.setBounds        (bottom.removeFromLeft (60));

    // Inspector 调试按钮（右下角，不影响主布局）
    inspectButton.setBounds (getLocalBounds().removeFromBottom (24).removeFromRight (110));
}
```

---

### 任务 H（暂缓）：每轨 STEM 1 / STEM 2 增益控件

设计稿里每轨有 STEM 1 / STEM 2 两个独立小滑杆，对应 `setTrackStemGain()`。
**暂缓原因**：横屏宽度有限（每轨只有 ~256px），先把主 gain slider 做好。
后续要加时按 V1.1 设计稿在每轨里再分两小列即可。

---

## 5. 完整改动文件清单

| 文件 | 改动类型 | 涉及任务 |
|---|---|---|
| `source/PluginEditor.cpp` | 主要修改：构造、paint、timerCallback、updateUIForMode、resized | A0 A B C D E G |
| `source/PluginEditor.h` | 删除 4 个 TextButton/ToggleButton，添加 5 个 DrawableButton + lambda 成员 | B C |
| `source/PluginProcessor.h` | 新增 `isTransportPlaying()` 声明 | C |
| `source/PluginProcessor.cpp` | 新增 `isTransportPlaying()` 实现，填 3 个 stub | C F |

**不需要改的文件**：`SeparationThread.h`、`SpliceThread.h`、`WaveformDisplay.h`、`CMakeLists.txt`、`source/remixing/*`。

---

## 6. 缺失素材清单（要从 V1.1 截图）

下面这些素材在现有 `assets/images/buttons/` 里没有，但 V1.1 设计稿里有。可以让 Evelyne 从 V1.1 PNG 按像素位置裁出来，或者请 Cursor 用图像工具直接裁。

| 素材名 | 用途 | 在 V1.1 里的大致位置 | 优先级 |
|---|---|---|---|
| `Settings_off.svg` / `Settings_hover.svg` | 主 transport 齿轮按钮 | 左下角 ⚙ 图标 | 中 |
| `Loop_main_off/on.svg`（可复用现有 Loop_*.svg）| 主 transport loop（小尺寸） | 齿轮右边 | 低 |
| `AutoSplice_off/on.png` | 主 transport 橙色 AUTO SPLICE 按钮 | 中下偏右 | 低 |
| `Regenerate_off/on.png` | 主 transport 灰色 REGENERATE 按钮 | 右下 | 低 |
| `Randomize_off/on.png` | 主 transport 灰色 RANDOMIZE 按钮 | 右下 | 低 |
| `Rec_off/on.png` | REC 红色方块按钮 | 最右下 | 低 |
| `Density_arrows.svg` | DENSITY 滑杆左右箭头 | BPM 旋钮右边 | 极低（先省略） |

**任务 D/E 完成时**：缺的图都用 JUCE 原生 `TextButton` + 自定义颜色占位，不影响功能。

---

## 7. 完成顺序

```
A0 (横屏尺寸)  →  A (背景图)  →  G (横屏 resized)
            ↓
B (Stem 面板隐藏 + Expertise 显示)
            ↓
C (主 transport SVG)  →  D (BPM 旋钮)  →  E (颜色主题)
            ↓
F (3 个 stub 填充)  →  缺失素材替换占位
```

预计总工时：**约 6~7 小时**（不含 demucs.onnx 部署和测试）。

---

## 8. 分支策略

所有 UX 改动在当前分支开发，改完后 PR 到 `dev`。
**不要修改** `SeparationThread.h` / `SpliceThread.h` / `remixing/` 内部算法——那些是其他人的模块，UX 只通过 `PluginProcessor` 的 public API 调用。
