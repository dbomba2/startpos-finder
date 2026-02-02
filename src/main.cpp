#include <Geode/Geode.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <string>
#include <algorithm>

using namespace geode::prelude;

// s_originalLevel: pinpoints the starting level
// s_isSearching: tells us if the mod was used to make the latest search
static Ref<GJGameLevel> s_originalLevel = nullptr;
static bool s_isSearching = false;

class $modify(StartPosFinder, LevelInfoLayer) {

    bool init(GJGameLevel * level, bool p1) {
        if (!LevelInfoLayer::init(level, p1)) return false;

        auto diffSprite = this->getChildByID("difficulty-sprite");
        if (!diffSprite) return true;

        ccColor3B btnColor = { 0, 255, 255 }; // Default: Cyan 

        // Checks if we're in search mode
        if (s_originalLevel && s_isSearching) {

            
            if (level == s_originalLevel) {
                s_isSearching = false;
                s_originalLevel = nullptr;
            }
            
            else {
                std::string origName = s_originalLevel->m_levelName;
                std::string currName = level->m_levelName;

                std::transform(origName.begin(), origName.end(), origName.begin(), ::tolower);
                std::transform(currName.begin(), currName.end(), currName.begin(), ::tolower);

                // Fix for long names who get cut off 
                size_t checkLen = std::min(origName.length(), size_t(10));
                std::string token = origName.substr(0, checkLen);

                // Checks if the startpos is related
                if (currName.find(token) != std::string::npos) {
                    btnColor = { 255, 150, 0 }; // Orange (go back)
                }
                else {
                    // Manual search bugfix
                    s_isSearching = false;
                    s_originalLevel = nullptr;
                }
            }
        }

        auto spr = CCSprite::createWithSpriteFrameName("GJ_practiceBtn_001.png");
        spr->setColor(btnColor);
        spr->setScale(0.5f);

        auto btn = CCMenuItemSpriteExtra::create(
            spr,
            this,
            menu_selector(StartPosFinder::onSmartSearch)
        );
        btn->setID("startpos-btn"_spr);

        auto menu = CCMenu::create();
        menu->setPosition(diffSprite->getPosition());
        menu->setContentSize({ 0, 0 });
        menu->setID("startpos-menu"_spr);

        float xOffset = (diffSprite->getScaledContentSize().width / 2) + 15.0f;
        btn->setPosition({ xOffset, 0 });

        menu->addChild(btn);
        this->addChild(menu);

        return true;
    }

    void onSmartSearch(CCObject * sender) {
        // "Go back" logic 
        if (s_originalLevel && m_level != s_originalLevel && s_isSearching) {

            // Gauntlet issues
            auto scene = LevelInfoLayer::scene(s_originalLevel, false);
            CCDirector::get()->replaceScene(CCTransitionFade::create(0.5f, scene));

            // Status reset
            s_originalLevel = nullptr;
            s_isSearching = false;
            return;
        }

        // "Save and search" logic
        s_originalLevel = m_level;
        s_isSearching = true;

        std::string levelName = m_level->m_levelName;

        // Trim 
        while (!levelName.empty() && std::isspace(levelName.back())) {
            levelName.pop_back();
        }

        levelName.erase(levelName.begin(), std::find_if(levelName.begin(), levelName.end(), [](unsigned char ch) {
            return !std::isspace(ch);
            }));

        std::string suffix = " startpos";
        const int MAX_LEN = 20;
        std::string finalQuery;

        if (levelName.length() >= MAX_LEN) {
            finalQuery = levelName.substr(0, MAX_LEN);
        }
        else {
            int spaceLeft = MAX_LEN - levelName.length();
            std::string partialSuffix = suffix.substr(0, spaceLeft);
            finalQuery = levelName + partialSuffix;
        }

        while (!finalQuery.empty() && std::isspace(finalQuery.back())) {
            finalQuery.pop_back();
        }

        log::info("Original save and startpos version for: '{}'", finalQuery);

        auto searchObj = GJSearchObject::create(SearchType::Search, finalQuery);
        auto browserScene = LevelBrowserLayer::scene(searchObj);
        CCDirector::get()->replaceScene(CCTransitionFade::create(0.5f, browserScene));
    }
};