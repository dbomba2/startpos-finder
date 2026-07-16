#include <Geode/Geode.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <string_view>

using namespace geode::prelude;

namespace {
    static Ref<GJGameLevel> s_originalLevel = nullptr;
    static bool s_isSearching = false;

    constexpr int kGridColumns = 5;
    constexpr int kGridRows = 3;

    constexpr float kSideUiReserve = 62.0f;
    constexpr float kTopUiReserve = 34.0f;
    constexpr float kBottomUiReserve = 28.0f;
    constexpr float kUiGap = 8.0f;
    constexpr float kClipPadding = 4.0f;

    struct GridSlot {
        std::string_view name;
        int column;
        int row;
    };

    // Used only by the orange return button in EditLevelLayer.
    constexpr std::array<GridSlot, 12> kGridSlots {{
        { "Top Left", 0, 2 },
        { "Top Inner Left", 1, 2 },
        { "Top Inner Right", 3, 2 },
        { "Top Right", 4, 2 },
        { "Middle Left", 0, 1 },
        { "Middle Inner Left", 1, 1 },
        { "Middle Inner Right", 3, 1 },
        { "Middle Right", 4, 1 },
        { "Bottom Left", 0, 0 },
        { "Bottom Inner Left", 1, 0 },
        { "Bottom Inner Right", 3, 0 },
        { "Bottom Right", 4, 0 },
    }};

    struct LocalRect {
        float left;
        float right;
        float bottom;
        float top;
    };

    static std::string toLower(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }

    static GridSlot const* findGridSlot(std::string_view name) {
        for (auto const& slot : kGridSlots) {
            if (slot.name == name) {
                return &slot;
            }
        }
        return nullptr;
    }

    static LocalRect getVisibleRectInNode(CCNode* node) {
        auto director = CCDirector::get();
        auto visibleOrigin = director->getVisibleOrigin();
        auto visibleSize = director->getVisibleSize();

        auto bottomLeft = node->convertToNodeSpace(visibleOrigin);
        auto topRight = node->convertToNodeSpace({
            visibleOrigin.x + visibleSize.width,
            visibleOrigin.y + visibleSize.height,
        });

        return {
            std::min(bottomLeft.x, topRight.x),
            std::max(bottomLeft.x, topRight.x),
            std::min(bottomLeft.y, topRight.y),
            std::max(bottomLeft.y, topRight.y),
        };
    }

    static CCPoint clampButtonCenter(
        CCNode* layer,
        CCPoint position,
        CCSize footprint
    ) {
        auto rect = getVisibleRectInNode(layer);
        float halfWidth = footprint.width * 0.5f;
        float halfHeight = footprint.height * 0.5f;

        float minX = rect.left + halfWidth + kClipPadding;
        float maxX = rect.right - halfWidth - kClipPadding;
        float minY = rect.bottom + halfHeight + kClipPadding;
        float maxY = rect.top - halfHeight - kClipPadding;

        position.x = minX <= maxX
            ? std::clamp(position.x, minX, maxX)
            : (rect.left + rect.right) * 0.5f;

        position.y = minY <= maxY
            ? std::clamp(position.y, minY, maxY)
            : (rect.bottom + rect.top) * 0.5f;

        return position;
    }

    static CCPoint getGridPosition(
        CCNode* layer,
        GridSlot const& slot,
        CCSize footprint
    ) {
        auto rect = getVisibleRectInNode(layer);
        float halfWidth = footprint.width * 0.5f;
        float halfHeight = footprint.height * 0.5f;

        float gridLeft = rect.left + kSideUiReserve + halfWidth + kUiGap;
        float gridRight = rect.right - kSideUiReserve - halfWidth - kUiGap;
        float gridBottom = rect.bottom + kBottomUiReserve + halfHeight + kUiGap;
        float gridTop = rect.top - kTopUiReserve - halfHeight - kUiGap;

        if (gridLeft > gridRight) {
            gridLeft = rect.left + halfWidth + kClipPadding;
            gridRight = rect.right - halfWidth - kClipPadding;
        }
        if (gridBottom > gridTop) {
            gridBottom = rect.bottom + halfHeight + kClipPadding;
            gridTop = rect.top - halfHeight - kClipPadding;
        }

        int column = std::clamp(slot.column, 0, kGridColumns - 1);
        int row = std::clamp(slot.row, 0, kGridRows - 1);

        float xFactor = static_cast<float>(column) / static_cast<float>(kGridColumns - 1);
        float yFactor = static_cast<float>(row) / static_cast<float>(kGridRows - 1);

        CCPoint position {
            gridLeft + (gridRight - gridLeft) * xFactor,
            gridBottom + (gridTop - gridBottom) * yFactor,
        };

        return clampButtonCenter(layer, position, footprint);
    }

    static CCPoint getConfiguredGridPosition(
        CCNode* layer,
        char const* settingKey,
        CCPoint automaticPosition,
        CCSize footprint
    ) {
        auto configured = Mod::get()->getSettingValue<std::string>(settingKey);

        if (configured == "Auto") {
            return clampButtonCenter(layer, automaticPosition, footprint);
        }

        if (auto slot = findGridSlot(configured)) {
            return getGridPosition(layer, *slot, footprint);
        }

        log::warn(
            "Unknown button position '{}' for setting '{}'; using Auto",
            configured,
            settingKey
        );
        return clampButtonCenter(layer, automaticPosition, footprint);
    }

    static CCPoint getVisibleCenterInNode(CCNode* node) {
        auto director = CCDirector::get();
        auto visibleOrigin = director->getVisibleOrigin();
        auto visibleSize = director->getVisibleSize();

        return node->convertToNodeSpace({
            visibleOrigin.x + visibleSize.width * 0.5f,
            visibleOrigin.y + visibleSize.height * 0.5f,
        });
    }

    static CCPoint getAutomaticLevelInfoPosition(
        LevelInfoLayer* layer,
        GJDifficultySprite* difficultySprite
    ) {
        if (difficultySprite && difficultySprite->getParent()) {
            auto worldPosition = difficultySprite->getParent()->convertToWorldSpace(
                difficultySprite->getPosition()
            );
            auto localPosition = layer->convertToNodeSpace(worldPosition);

            return {
                localPosition.x + difficultySprite->getScaledContentSize().width * 0.5f + 20.0f,
                localPosition.y,
            };
        }

        auto center = getVisibleCenterInNode(layer);
        return { center.x - 160.0f, center.y + 40.0f };
    }

    static CCPoint getAutomaticEditPosition(EditLevelLayer* layer) {
        auto center = getVisibleCenterInNode(layer);
        return { center.x - 185.0f, center.y - 23.0f };
    }

    // Curated LevelInfoLayer presets based on the original user complaint.
    // These are deliberately not a full-screen mathematical grid: every slot
    // is chosen to avoid the play button, progress bars, title, and side menus.
    static CCPoint getLevelInfoPresetPosition(
        LevelInfoLayer* layer,
        std::string const& configured,
        CCSize footprint,
        GJDifficultySprite* difficultySprite
    ) {
        auto rect = getVisibleRectInNode(layer);

        auto place = [&](float x, float y) {
            return clampButtonCenter(layer, { x, y }, footprint);
        };

        auto automaticPosition = clampButtonCenter(
            layer,
            getAutomaticLevelInfoPosition(layer, difficultySprite),
            footprint
        );

        if (configured == "Auto" || configured == "Right of Difficulty") {
            return automaticPosition;
        }

        // Place the button immediately to the right of the vanilla play button.
        // m_playSprite is used as the visual reference, so the placement follows
        // the actual play-button position and scale on different resolutions.
        if (configured == "Right Side of Play Button") {
            if (layer->m_playSprite) {
                auto playRightCenterWorld = layer->m_playSprite->convertToWorldSpace({
                    layer->m_playSprite->getContentSize().width,
                    layer->m_playSprite->getContentSize().height * 0.5f,
                });
                auto playRightCenter = layer->convertToNodeSpace(playRightCenterWorld);
                constexpr float kPlayButtonGap = 2.0f;

                return place(
                    playRightCenter.x + footprint.width * 0.5f + kPlayButtonGap,
                    playRightCenter.y
                );
            }

            log::warn("Unable to find the play sprite; using Auto placement");
            return automaticPosition;
        }

        // Left edge placements, similar to the marked positions in the report.
        if (configured == "Left Top") {
            return place(rect.left + 55.0f, rect.top - 108.0f);
        }
        if (configured == "Left Middle") {
            return place(rect.left + 55.0f, rect.top - 164.0f);
        }
        if (configured == "Left Bottom") {
            return place(rect.left + 55.0f, rect.bottom + 72.0f);
        }

        // Inner-left placements around the difficulty area without covering it.
        if (configured == "Difficulty Upper") {
            return place(rect.left + 145.0f, rect.top - 105.0f);
        }
        if (configured == "Difficulty Lower") {
            return place(rect.left + 145.0f, rect.top - 166.0f);
        }

        // Right-side placements around the stats column.
        if (configured == "Stats Upper") {
            return place(rect.right - 145.0f, rect.top - 142.0f);
        }
        if (configured == "Stats Lower") {
            return place(rect.right - 145.0f, rect.top - 205.0f);
        }

        // Bottom-right placements near the folder / navigation arrows.
        if (configured == "Above Arrows") {
            return place(rect.right - 103.0f, rect.bottom + 102.0f);
        }
        if (configured == "Bottom Right") {
            return place(rect.right - 103.0f, rect.bottom + 62.0f);
        }

        log::warn("Unknown LevelInfo button position '{}'; using Auto", configured);
        return automaticPosition;
    }

    static CCMenu* createPositioningMenu(std::string const& id) {
        auto menu = CCMenu::create();
        menu->setID(id);
        menu->setPosition({ 0.0f, 0.0f });
        return menu;
    }
}

class $modify(StartPosFinder, LevelInfoLayer) {
    bool init(GJGameLevel* level, bool challenge) {
        if (!LevelInfoLayer::init(level, challenge)) {
            return false;
        }

        ccColor3B buttonColor = { 0, 255, 255 };
        bool isRelated = false;

        auto difficultySprite = m_difficultySprite;
        bool isRatedLevel = difficultySprite && difficultySprite->getParent();

        if (s_originalLevel && s_isSearching) {
            if (level == s_originalLevel) {
                s_isSearching = false;
                s_originalLevel = nullptr;
            }
            else {
                std::string originalName = toLower(s_originalLevel->m_levelName);
                std::string currentName = toLower(level->m_levelName);
                std::string token = originalName.substr(
                    0,
                    std::min(originalName.length(), size_t(10))
                );

                if (currentName.find(token) != std::string::npos) {
                    isRelated = true;
                }

                if (!isRatedLevel) {
                    isRelated = true;
                }

                if (isRelated) {
                    buttonColor = { 255, 150, 0 };
                }
                else {
                    s_isSearching = false;
                    s_originalLevel = nullptr;
                }
            }
        }

        auto sprite = CCSprite::createWithSpriteFrameName("GJ_practiceBtn_001.png");
        if (!sprite) {
            return true;
        }

        sprite->setColor(buttonColor);
        sprite->setScale(0.6f);
        auto footprint = sprite->getScaledContentSize();

        auto button = CCMenuItemSpriteExtra::create(
            sprite,
            this,
            menu_selector(StartPosFinder::onSmartSearch)
        );
        button->setID("startpos-finder-button"_spr);

        // Reuse the existing vanilla play-button menu. This is the compatibility
        // fix that keeps the green play button and avoids extra top-level menus.
        auto menu = m_playBtnMenu;
        if (!menu) {
            menu = createPositioningMenu("startpos-finder-menu"_spr);
            this->addChild(menu, 100);
        }

        menu->addChild(button, 100);

        auto configured = Mod::get()->getSettingValue<std::string>(
            "level-info-position"
        );

        auto layerPosition = getLevelInfoPresetPosition(
            this,
            configured,
            footprint,
            difficultySprite
        );

        // Presets are expressed in LevelInfoLayer coordinates; convert them to
        // the existing menu's local coordinates before assigning the position.
        auto worldPosition = this->convertToWorldSpace(layerPosition);
        button->setPosition(menu->convertToNodeSpace(worldPosition));

        return true;
    }

    void onSmartSearch(CCObject*) {
        auto dispatcher = CCDirector::sharedDirector()->getKeyboardDispatcher();
        bool isShiftPressed = dispatcher->getShiftKeyPressed();

        if (s_originalLevel && m_level != s_originalLevel && s_isSearching) {
            if (isShiftPressed) {
                s_originalLevel = nullptr;
                s_isSearching = false;
                Notification::create(
                    "Tracking Reset",
                    NotificationIcon::Info
                )->show();
                return;
            }

            auto scene = LevelInfoLayer::scene(s_originalLevel, false);
            CCDirector::get()->replaceScene(
                CCTransitionFade::create(0.5f, scene)
            );
            return;
        }

        s_originalLevel = m_level;
        s_isSearching = true;

        if (isShiftPressed) {
            auto searchObject = GJSearchObject::create(SearchType::MyLevels);
            auto scene = LevelBrowserLayer::scene(searchObject);
            CCDirector::get()->replaceScene(
                CCTransitionFade::create(0.5f, scene)
            );
        }
        else {
            this->performSmartSearch(m_level->m_levelName);
        }
    }

    void performSmartSearch(std::string levelName) {
        while (!levelName.empty() && std::isspace(
            static_cast<unsigned char>(levelName.back())
        )) {
            levelName.pop_back();
        }

        levelName.erase(
            levelName.begin(),
            std::find_if(
                levelName.begin(),
                levelName.end(),
                [](unsigned char character) {
                    return !std::isspace(character);
                }
            )
        );

        std::string suffix = " startpos";
        constexpr int kMaxLength = 20;
        std::string finalQuery;

        if (levelName.length() >= kMaxLength) {
            finalQuery = levelName.substr(0, kMaxLength);
        }
        else {
            int spaceLeft = kMaxLength - static_cast<int>(levelName.length());
            finalQuery = levelName + suffix.substr(0, static_cast<size_t>(spaceLeft));
        }

        while (!finalQuery.empty() && std::isspace(
            static_cast<unsigned char>(finalQuery.back())
        )) {
            finalQuery.pop_back();
        }

        auto searchObject = GJSearchObject::create(SearchType::Search, finalQuery);
        auto browserScene = LevelBrowserLayer::scene(searchObject);
        CCDirector::get()->replaceScene(
            CCTransitionFade::create(0.5f, browserScene)
        );
    }
};

class $modify(StartPosFinderEdit, EditLevelLayer) {
    bool init(GJGameLevel* level) {
        if (!EditLevelLayer::init(level)) {
            return false;
        }

        if (s_originalLevel && s_isSearching) {
            auto sprite = CCSprite::createWithSpriteFrameName("GJ_practiceBtn_001.png");
            if (!sprite) {
                return true;
            }

            sprite->setColor({ 255, 150, 0 });
            sprite->setScale(0.7f);
            auto footprint = sprite->getScaledContentSize();

            auto button = CCMenuItemSpriteExtra::create(
                sprite,
                this,
                menu_selector(StartPosFinderEdit::onBackToOriginal)
            );
            button->setID("startpos-finder-return-button"_spr);

            auto menu = createPositioningMenu("startpos-finder-return-menu"_spr);
            menu->addChild(button);
            this->addChild(menu, 9999);

            button->setPosition(getConfiguredGridPosition(
                this,
                "edit-level-position",
                getAutomaticEditPosition(this),
                footprint
            ));
        }

        return true;
    }

    void onBackToOriginal(CCObject*) {
        if (s_originalLevel) {
            auto scene = LevelInfoLayer::scene(s_originalLevel, false);
            CCDirector::get()->replaceScene(
                CCTransitionFade::create(0.5f, scene)
            );
        }
    }
};
