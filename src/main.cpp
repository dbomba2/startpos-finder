#include <Geode/Geode.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include <string>
#include <algorithm>
#include <cctype>

using namespace geode::prelude;

static Ref<GJGameLevel> s_originalLevel = nullptr;
static bool s_isSearching = false;

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return (unsigned char)std::tolower(c);
        });
    return s;
}

class $modify(StartPosFinder, LevelInfoLayer) {

    bool init(GJGameLevel * level, bool p1) {
        if (!LevelInfoLayer::init(level, p1)) return false;

        ccColor3B btnColor = { 0, 255, 255 };
        bool isRelated = false;

        auto diffSprite = this->getChildByID("difficulty-sprite");
        bool isRatedLevel = (diffSprite != nullptr);

        if (s_originalLevel && s_isSearching) {
            if (level == s_originalLevel) {
                s_isSearching = false;
                s_originalLevel = nullptr;
            }
            else {
                std::string origName = toLower(s_originalLevel->m_levelName);
                std::string currName = toLower(level->m_levelName);
                std::string token = origName.substr(0, std::min(origName.length(), size_t(10)));

                if (currName.find(token) != std::string::npos) {
                    isRelated = true;
                }

                if (!isRatedLevel) {
                    isRelated = true;
                }

                if (isRelated) {
                    btnColor = { 255, 150, 0 };
                }
                else {
                    s_isSearching = false;
                    s_originalLevel = nullptr;
                }
            }
        }

        auto spr = CCSprite::createWithSpriteFrameName("GJ_practiceBtn_001.png");
        if (!spr) return true;

        spr->setColor(btnColor);
        spr->setScale(0.6f);

        auto btn = CCMenuItemSpriteExtra::create(
            spr, this, menu_selector(StartPosFinder::onSmartSearch)
        );

        auto menu = CCMenu::create();
        menu->setPosition({ 0, 0 });

        this->addChild(menu, 100);
        menu->addChild(btn);

        auto winSize = CCDirector::get()->getWinSize();

        if (isRatedLevel) {
            CCPoint worldPos = diffSprite->getParent()->convertToWorldSpace(diffSprite->getPosition());
            CCPoint localPos = this->convertToNodeSpace(worldPos);

            btn->setPosition({ localPos.x + (diffSprite->getScaledContentSize().width / 2.0f) + 20.0f, localPos.y });
        }
        else {
            float targetX = (winSize.width / 2.0f) - 160.0f;
            float targetY = (winSize.height / 2.0f) + 40.0f;

            btn->setPosition({ targetX, targetY });
        }

        return true;
    }

    void onSmartSearch(CCObject * sender) {
        auto dispatcher = CCDirector::sharedDirector()->getKeyboardDispatcher();
        bool isShiftPressed = dispatcher->getShiftKeyPressed();

        if (s_originalLevel && m_level != s_originalLevel && s_isSearching) {
            if (isShiftPressed) {
                s_originalLevel = nullptr;
                s_isSearching = false;
                Notification::create("Tracking Reset", NotificationIcon::Info)->show();
                return;
            }

            auto scene = LevelInfoLayer::scene(s_originalLevel, false);
            CCDirector::get()->replaceScene(CCTransitionFade::create(0.5f, scene));
            return;
        }

        s_originalLevel = m_level;
        s_isSearching = true;

        if (isShiftPressed) {
            auto searchObj = GJSearchObject::create(SearchType::MyLevels);
            auto scene = LevelBrowserLayer::scene(searchObj);
            CCDirector::get()->replaceScene(CCTransitionFade::create(0.5f, scene));
        }
        else {
            this->performSmartSearch(m_level->m_levelName);
        }
    }

    void performSmartSearch(std::string levelName) {
        while (!levelName.empty() && std::isspace((unsigned char)levelName.back()))
            levelName.pop_back();

        levelName.erase(levelName.begin(), std::find_if(levelName.begin(), levelName.end(),
            [](unsigned char ch) { return !std::isspace(ch); }
        ));

        std::string suffix = " startpos";
        const int MAX_LEN = 20;
        std::string finalQuery;

        if (levelName.length() >= MAX_LEN) {
            finalQuery = levelName.substr(0, MAX_LEN);
        }
        else {
            int spaceLeft = MAX_LEN - (int)levelName.length();
            finalQuery = levelName + suffix.substr(0, (size_t)spaceLeft);
        }

        while (!finalQuery.empty() && std::isspace((unsigned char)finalQuery.back()))
            finalQuery.pop_back();

        auto searchObj = GJSearchObject::create(SearchType::Search, finalQuery);
        auto browserScene = LevelBrowserLayer::scene(searchObj);
        CCDirector::get()->replaceScene(CCTransitionFade::create(0.5f, browserScene));
    }
};

class $modify(StartPosFinderEdit, EditLevelLayer) {
    bool init(GJGameLevel * level) {
        if (!EditLevelLayer::init(level)) return false;

        if (s_originalLevel && s_isSearching) {
            auto winSize = CCDirector::get()->getWinSize();

            auto spr = CCSprite::createWithSpriteFrameName("GJ_practiceBtn_001.png");
            if (!spr) return true;

            spr->setColor({ 255, 150, 0 });
            spr->setScale(0.6f);

            auto btn = CCMenuItemSpriteExtra::create(
                spr, this, menu_selector(StartPosFinderEdit::onBackToOriginal)
            );

            auto menu = CCMenu::create();
            menu->setPosition({ 45.f, winSize.height - 90.f });
            menu->addChild(btn);

            this->addChild(menu, 9999);
        }

        return true;
    }

    void onBackToOriginal(CCObject * sender) {
        if (s_originalLevel) {
            auto scene = LevelInfoLayer::scene(s_originalLevel, false);
            CCDirector::get()->replaceScene(CCTransitionFade::create(0.5f, scene));
        }
    }
};