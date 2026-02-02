#include <Geode/Geode.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <string>
#include <algorithm>

using namespace geode::prelude;

class $modify(StartPosFinder, LevelInfoLayer) {

    bool init(GJGameLevel * level, bool p1) {
        // 1. Esegui il codice originale del gioco
        if (!LevelInfoLayer::init(level, p1)) return false;

        // 2. Trova l'icona della difficoltà
        auto diffSprite = this->getChildByID("difficulty-sprite");
        if (!diffSprite) return true;

        // 3. Crea lo sprite del pulsante
        auto spr = CCSprite::createWithSpriteFrameName("GJ_practiceBtn_001.png");
        spr->setColor({ 0, 255, 255 }); // Cyan
        spr->setScale(0.5f);

        // 4. Crea il pulsante
        auto btn = CCMenuItemSpriteExtra::create(
            spr,
            this,
            menu_selector(StartPosFinder::onSmartSearch)
        );
        btn->setID("startpos-btn"_spr);

        // 5. Menu dedicato per il posizionamento
        auto menu = CCMenu::create();
        menu->setPosition(diffSprite->getPosition());
        menu->setContentSize({ 0, 0 });
        menu->setID("startpos-menu"_spr);

        // 6. Calcolo offset
        float xOffset = (diffSprite->getScaledContentSize().width / 2) + 15.0f;
        btn->setPosition({ xOffset, 0 });

        // 7. Aggiunta alla scena
        menu->addChild(btn);
        this->addChild(menu);

        return true;
    }

    void onSmartSearch(CCObject * sender) {
        std::string levelName = m_level->m_levelName;

        // --- RIMOZIONE SPAZI IN ECCESSO (TRIM) ---
        // Rimuove spazi alla fine del nome del livello
        while (!levelName.empty() && std::isspace(levelName.back())) {
            levelName.pop_back();
        }
        // Rimuove spazi all'inizio (meno comune, ma possibile)
        levelName.erase(levelName.begin(), std::find_if(levelName.begin(), levelName.end(), [](unsigned char ch) {
            return !std::isspace(ch);
            }));

        std::string suffix = " startpos";
        const int MAX_LEN = 20;

        std::string finalQuery;

        // --- LOGICA DI COSTRUZIONE QUERY OTTIMIZZATA ---
        if (levelName.length() >= MAX_LEN) {
            // Se il nome è già di 20 o più caratteri, prendiamo solo i primi 20
            finalQuery = levelName.substr(0, MAX_LEN);
        }
        else {
            // Altrimenti, vediamo quanto spazio rimane per il suffisso
            int spaceLeft = MAX_LEN - levelName.length();

            // Aggiungiamo solo la parte di " startpos" che entra nel limite
            std::string partialSuffix = suffix.substr(0, spaceLeft);
            finalQuery = levelName + partialSuffix;
        }

        // Rimuove eventuali spazi rimasti alla fine dopo il troncamento (es. "Yatagarasu ")
        while (!finalQuery.empty() && std::isspace(finalQuery.back())) {
            finalQuery.pop_back();
        }

        log::info("Ricerca pulita per: '{}'", finalQuery);

        auto searchObj = GJSearchObject::create(SearchType::Search, finalQuery);
        auto browserScene = LevelBrowserLayer::scene(searchObj);
        CCDirector::get()->replaceScene(CCTransitionFade::create(0.5f, browserScene));
    }
};