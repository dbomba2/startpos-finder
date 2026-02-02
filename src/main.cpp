#include <Geode/Geode.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <string>
#include <algorithm>

using namespace geode::prelude;

// Variabili globali statiche per gestire lo stato della navigazione
// s_originalLevel: tiene in memoria il puntatore al livello da cui siamo partiti
// s_isSearching: ci dice se la mod e' "attiva" in una sequenza di ricerca
static Ref<GJGameLevel> s_originalLevel = nullptr;
static bool s_isSearching = false;

class $modify(StartPosFinder, LevelInfoLayer) {

    bool init(GJGameLevel * level, bool p1) {
        if (!LevelInfoLayer::init(level, p1)) return false;

        auto diffSprite = this->getChildByID("difficulty-sprite");
        if (!diffSprite) return true;

        ccColor3B btnColor = { 0, 255, 255 }; // Default: Ciano (Cerca)

        // --- LOGICA DI CONTROLLO "PARENTELA" ---
        // Verifica se siamo in modalita' ricerca e se c'e' un livello salvato
        if (s_originalLevel && s_isSearching) {

            // Caso 1: Siamo tornati sul livello originale? -> Resetta la ricerca
            if (level == s_originalLevel) {
                s_isSearching = false;
                s_originalLevel = nullptr;
            }
            // Caso 2: Siamo su un livello diverso. Controlliamo se e' correlato.
            else {
                std::string origName = s_originalLevel->m_levelName;
                std::string currName = level->m_levelName;

                // Normalizziamo i nomi (tutto minuscolo) per il confronto
                std::transform(origName.begin(), origName.end(), origName.begin(), ::tolower);
                std::transform(currName.begin(), currName.end(), currName.begin(), ::tolower);

                // Prendiamo una porzione significativa del nome originale (primi 10 caratteri)
                // Questo evita problemi con nomi lunghi troncati
                size_t checkLen = std::min(origName.length(), size_t(10));
                std::string token = origName.substr(0, checkLen);

                // Se il livello attuale contiene il token del nome originale, e' uno startpos correlato
                if (currName.find(token) != std::string::npos) {
                    btnColor = { 255, 150, 0 }; // Arancione (Torna Indietro)
                }
                else {
                    // SE NON C'ENTRA NULLA (es. l'utente ha cercato altro a mano, tipo Acheron dopo Bloodbath)
                    // -> RESETTA TUTTO! La mod dimentica il passato e si prepara a cercare per il nuovo livello.
                    s_isSearching = false;
                    s_originalLevel = nullptr;
                    // Il colore rimane Ciano
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
        // --- LOGICA "TORNA INDIETRO" ---
        // Se siamo in modalita' ricerca, c'e' un originale e siamo su un livello diverso (e init non ha resettato)
        if (s_originalLevel && m_level != s_originalLevel && s_isSearching) {

            // Torna alla scena del livello originale
            // 'false' indica che non siamo in un menu Gauntlet
            auto scene = LevelInfoLayer::scene(s_originalLevel, false);
            CCDirector::get()->replaceScene(CCTransitionFade::create(0.5f, scene));

            // Resetta lo stato: missione compiuta, siamo tornati a casa
            s_originalLevel = nullptr;
            s_isSearching = false;
            return;
        }

        // --- LOGICA "SALVA E CERCA" ---
        // Impostiamo il nuovo livello originale e attiviamo il flag di ricerca
        s_originalLevel = m_level;
        s_isSearching = true;

        std::string levelName = m_level->m_levelName;

        // Trim spazi finali
        while (!levelName.empty() && std::isspace(levelName.back())) {
            levelName.pop_back();
        }
        // Trim spazi iniziali
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

        log::info("Salvataggio originale e ricerca startpos per: '{}'", finalQuery);

        auto searchObj = GJSearchObject::create(SearchType::Search, finalQuery);
        auto browserScene = LevelBrowserLayer::scene(searchObj);
        CCDirector::get()->replaceScene(CCTransitionFade::create(0.5f, browserScene));
    }
};