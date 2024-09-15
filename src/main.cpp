#include <Geode/Geode.hpp>


#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/CustomSongWidget.hpp>
#include <Geode/modify/CreatorLayer.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/ui/BasedButtonSprite.hpp>
#include <Geode/loader/SettingEvent.hpp>
#include <filesystem>
#include <Geode/modify/MusicDownloadManager.hpp>

#ifdef GEODE_IS_ANDROID
bool android = true;
#else
bool android = false;
#endif

using namespace geode::prelude;

CustomSongWidget* songWidget = nullptr;
CCMenuItemSpriteExtra* trashButton = nullptr;
CCNode* downloadButton = nullptr;
CCNode* cancelButton = nullptr;
CCMenuItemSpriteExtra* settingsButton = nullptr;
bool deleteSongs = false;
bool deleteSFX = false;
int songCount = 0;
bool isPlaying = false;
int waitTime = 0;
GJGameLevel* globalLevel = nullptr;
LevelInfoLayer* globalLayer = nullptr;
std::filesystem::path macSaveDir;

void getAndDeleteAudio(GJGameLevel* level, bool sfx, std::string songIDs, std::string sfxIDs) {
    if (!sfx) songCount = 0;
    if (!sfx && songIDs == "" && sfxIDs == "") { 
        trashButton->setVisible(false);
        return songWidget->deleteSong();
    }
    else if (!sfx && (songIDs == "" || !deleteSongs)) return trashButton->setVisible(false); 
    else if (sfx && (sfxIDs == "" || !deleteSFX)) return trashButton->setVisible(false); 
    std::string audioIDs = (sfx) ? sfxIDs : songIDs;
    std::stringstream ss(audioIDs);
    std::vector<std::string> tokens;
    std::string token;
    while (std::getline(ss, token, ',')) {
    tokens.push_back(token);
    }
    for (int i = 0; i < tokens.size(); i++) {
        std::string filename;
        if (sfx) {
            filename = MusicDownloadManager::sharedState()->pathForSFX(std::stoi(tokens[i]));	
        }
        else {
            filename = MusicDownloadManager::sharedState()->pathForSong(std::stoi(tokens[i]));
            songCount++;
        }
        std::filesystem::remove(filename);
    }
}

void deleteAudio(GJGameLevel* level, LevelInfoLayer* layer) {
    trashButton->setVisible(false);
    std::string songIDs = std::string(level->m_songIDs);
    std::string sfxIDs = std::string(level->m_sfxIDs);
    if ((songIDs) == "" && sfxIDs == "")
        return songWidget->deleteSong();

    if (isPlaying && deleteSongs) {
        FMODAudioEngine::sharedEngine()->stopAllMusic();
        isPlaying = false;
        waitTime = 10;
        return;
    }

    getAndDeleteAudio(level, false, songIDs, sfxIDs);
    getAndDeleteAudio(level, true, songIDs, sfxIDs);

    auto newInfoLayer = LevelInfoLayer::create(level, false);
    globalLayer->removeFromParentAndCleanup(true);
    CCDirector::sharedDirector()->getRunningScene()->addChild(newInfoLayer);
    
    bool songs = (level->m_songIDs != "" && deleteSongs);
    bool sfx = (level->m_sfxIDs != "" && deleteSFX);
    bool all = (songs && sfx);
    bool none = (!songs && !sfx);
    bool robtopSFX = (songWidget->m_isRobtopSong && level->m_songIDs != "");
    std::string errorText = (all) ? "Songs & SFX Deleted" : ((songs) ? (songCount == 1) ? "Song Deleted" : "Songs Deleted" : ((sfx || robtopSFX) ? "SFX Deleted" : ""));

    songWidget->m_errorLabel->setColor(ccc3(255, 100, 0));
    songWidget->m_errorLabel->setString(errorText.c_str());
    songWidget->m_errorLabel->setVisible(true);
    songWidget->m_errorLabel->setScale(0.4f);

    deleteSongs = false;
    deleteSFX = false;
    songCount = 0;
    }

class trashPopup : public geode::Popup<> {
    bool song = false;
    bool sfx = false;
    bool autoSong = (Mod::get()->getSettingValue<bool>("auto-select-songs"));
    bool autoSFX = (Mod::get()->getSettingValue<bool>("auto-select-sfx"));
    CCMenuItemToggler* songToggle;
    CCMenuItemToggler* sfxToggle;
    protected:
        bool setup() override {
            m_closeBtn->setVisible(false);
            this->setTitle("Delete Audio");
            this->m_title->setScale(1);
            auto popupMenu = CCMenu::create();
            popupMenu->setPosition({175, 106.5});
            this->m_mainLayer->addChild(popupMenu);

            auto songSpriteOn = CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png");
            auto songSpriteOff = CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png");
            songToggle = CCMenuItemToggler::create(songSpriteOff, songSpriteOn, this, menu_selector(trashPopup::toggleSong));
            popupMenu->addChild(songToggle);
            songToggle->setPosition({-59, 5.5});
            if (autoSong) songToggle->toggle(true);

            auto sfxSpriteOn = CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png");
            auto sfxSpriteOff = CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png");
            sfxToggle = CCMenuItemToggler::create(sfxSpriteOff, sfxSpriteOn, this, menu_selector(trashPopup::toggleSFX));
            popupMenu->addChild(sfxToggle);
            sfxToggle->setPosition({60.5, 5.5});
            if (autoSFX) sfxToggle->toggle(true);

            auto cancelSprite = ButtonSprite::create("Cancel");
            auto cancelBtn = CCMenuItemSpriteExtra::create(cancelSprite, this, menu_selector(trashPopup::onCancel));
            auto deleteSprite = ButtonSprite::create("Delete");
            auto deleteBtn = CCMenuItemSpriteExtra::create(deleteSprite, this, menu_selector(trashPopup::onDelete));
            popupMenu->addChild(cancelBtn);
            popupMenu->addChild(deleteBtn);
            cancelBtn->setPosition({-59, -76.5});
            deleteBtn->setPosition({60.5, -76.5});

            auto messageText = CCLabelBMFont::create("What do you want to delete?", "chatFont.fnt");
            for (int i = 20; i < 26; i++) {
                auto letter = static_cast<CCSprite*>(messageText->getChildren()->objectAtIndex(i));
                letter->setColor(ccc3(255, 90, 90));
            }

            auto songText = CCLabelBMFont::create("Songs", "bigFont.fnt");
            auto sfxText = CCLabelBMFont::create("SFX", "bigFont.fnt");
            popupMenu->addChild(messageText);
            popupMenu->addChild(songText);
            popupMenu->addChild(sfxText);
            messageText->setPosition({0, 51.5});
            songText->setPosition({-59, -27.5});
            songText->setScale(0.7);
            sfxText->setPosition({60.5, -27.5});
            sfxText->setScale(0.7);

            if (!Mod::get()->getSettingValue<bool>("hide-settings-button")) {
                auto settingsSprite = CircleButtonSprite::createWithSpriteFrameName(
                    "geode.loader/settings.png", 
                    1.f, 
                    CircleBaseColor::DarkPurple, 
                    CircleBaseSize::Small
                );
                settingsSprite->setScale(0.75);
                settingsButton = CCMenuItemSpriteExtra::create(settingsSprite, this, menu_selector(trashPopup::openSettings));
                popupMenu->addChild(settingsButton);
                settingsButton->setPosition({-146, 77});
            }
            
            return true;
        }

    void toggleSong(CCObject* obj) {
        song = !song;
    }

    void toggleSFX(CCObject* obj) {
        sfx = !sfx;
    }

    void onCancel(CCObject* obj) {
        this->keyBackClicked();
    }

    void onDelete(CCObject* obj) {
        deleteSongs = (songToggle->isToggled() ? true : false);
        deleteSFX = (sfxToggle->isToggled() ? true : false);
        deleteAudio(layer->m_level, layer);
        this->keyBackClicked();
    }

    void openSettings(CCObject* obj) {
        geode::openSettingsPopup(Mod::get());
    }

    public:
        LevelInfoLayer* layer;
        static trashPopup* create() {
            auto ret = new trashPopup;
            if (ret->initAnchored(350, 213, "square01_001.png", CCRectZero)) {
                ret->autorelease();
                return ret;
            }
            delete ret;
            return nullptr;
        }
};

class $modify(Button, LevelInfoLayer) {
    
    void openSettings(CCObject* obj) {
        geode::openSettingsPopup(Mod::get());
    }

    void onPlay(CCObject* sender) {
        LevelInfoLayer::onPlay(sender);
        isPlaying = false;
    }

    void button(CCObject* obj) {
        if (songWidget->m_isRobtopSong) return;
        globalLevel = this->m_level;
        if (std::string(this->m_level->m_levelString) == "") return;
        std::string popupText = "Do you want to <cr>delete</c> ";
        if (songWidget->m_isRobtopSong) {
            popupText += "all SFX?";
            deleteSFX = true;
        }
        else {
            popupText += (std::string(this->m_level->m_songIDs) == "") ? "this song" : "all songs";
            popupText += (std::string(this->m_level->m_sfxIDs) != "") ? " and SFX?" : "?"; 
        }
            
        if ((!songWidget->m_isRobtopSong && std::string(this->m_level->m_sfxIDs) != "") || (std::string(this->m_level->m_songIDs) != "")) {
            auto popup = trashPopup::create();
            popup->layer = this;
            popup->show();
            return;
        }
        else {
            geode::createQuickPopup(
                "Delete Audio", 
                popupText,
                "Cancel", "Delete",
                [this](auto, bool btn2) {
                    if (btn2)  {
                        deleteAudio(this->m_level, this);
                    }
                }
            );
        }

        if (!Mod::get()->getSettingValue<bool>("hide-settings-button")) {
            auto settingsSprite = CircleButtonSprite::createWithSpriteFrameName(
                "geode.loader/settings.png", 
                1.f, 
                CircleBaseColor::DarkPurple, 
                CircleBaseSize::Small
            );
            settingsButton = CCMenuItemSpriteExtra::create(settingsSprite, this, menu_selector(Button::openSettings));
            settingsSprite->setScale(0.75);
            auto settingsMenu = CCMenu::create();
            CCArray* children = CCDirector::sharedDirector()->getRunningScene()->getChildren();
            auto popup = static_cast<FLAlertLayer*>(children->lastObject());
            popup->m_buttonMenu->addChild(settingsButton);
            settingsButton->setPosition({-147, 81});
            settingsButton->setZOrder(1);	
        }
    }

    bool init(GJGameLevel* level, bool challenge) {
        if (!LevelInfoLayer::init(level, challenge)) return false;

        songWidget = this->m_songWidget;
        downloadButton = songWidget->m_downloadBtn;
        cancelButton = songWidget->m_cancelDownloadBtn;
        globalLevel = this->m_level;
        globalLayer = this;

        auto sprite = CCSprite::createWithSpriteFrameName("GJ_trashBtn_001.png");
        sprite->setScale(0.8);
        trashButton = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(Button::button));
        trashButton->setPosition({128, -137});
        trashButton->setID("trash-button"_spr);
    
        auto trashMenu = CCMenu::create();
        trashMenu->setID("trash-menu"_spr);
        this->addChild(trashMenu);
        trashMenu->addChild(trashButton);
        trashButton->setVisible(false);

        if (!downloadButton->isVisible() && !cancelButton->isVisible()) {
            trashButton->setVisible(true);
        }	
        if ((songWidget->m_isRobtopSong && globalLevel->m_sfxIDs == "")) trashButton->setVisible(false);

        return true;
    }

    void onBack(CCObject* obj) {
        LevelInfoLayer::onBack(obj);
        songWidget = nullptr;
        trashButton = nullptr;
        downloadButton = nullptr;
        cancelButton = nullptr;
        settingsButton = nullptr;
        globalLevel = nullptr;
        globalLayer = nullptr;
        songCount = 0;
    }

    void levelDownloadFinished(GJGameLevel* level) {
        LevelInfoLayer::levelDownloadFinished(level);
        if (trashButton) trashButton->setVisible(!downloadButton->isVisible() && !cancelButton->isVisible());
        if (!songWidget) return;
        if (songWidget->m_isRobtopSong && level->m_sfxIDs == "") trashButton->setVisible(false);
    }
};

class $modify (CustomSongWidget) {
    virtual void downloadSongFinished(int i) {
        CustomSongWidget::downloadSongFinished(i);
        if (this == songWidget && trashButton && ((!downloadButton->isVisible() && !cancelButton->isVisible()) || std::string(this->m_errorLabel->getString()) == "Download complete.")) 
            trashButton->setVisible(true);
    }

    virtual void downloadAssetFinished(int i, GJAssetType assetType) {
        CustomSongWidget::downloadAssetFinished(i, assetType);
        if (assetType == GJAssetType::SFX && this == songWidget && trashButton && ((!downloadButton->isVisible() && !cancelButton->isVisible()) || std::string(this->m_errorLabel->getString()) == "Download complete.")) 
            trashButton->setVisible(true);
    }

    void onPlayback(CCObject* obj) {
        CustomSongWidget::onPlayback(obj);
        isPlaying = true;
    }
};

class $modify (CreatorLayer) {
    void onBack(CCObject* sender) {
        CreatorLayer::onBack(sender);
        isPlaying = false;
    }

    void onSecretVault(CCObject* sender) {
        CreatorLayer::onSecretVault(sender);
        isPlaying = false;
    }

    void onTreasureRoom(CCObject* sender) {
        CreatorLayer::onTreasureRoom(sender);
        isPlaying = false;
    }

};

class $modify (CCScheduler) {
    void update(float f) {
        CCScheduler::update(f);
        if (waitTime != 0) {
            waitTime--;
            if (waitTime == 0) {
                deleteAudio(globalLevel, globalLayer);
            }
        }
    }

};

$execute {
    geode::listenForSettingChanges("hide-settings-button", +[](bool  value) {
        if (settingsButton)
            settingsButton->setVisible((!value));
});
};

/*class $modify (MusicDownloadManager) {
    gd::string pathForSFX(int id) {
        auto path = MusicDownloadManager::pathForSFX(id);
        log::debug("pathForSFX: {}", path);
        return path;
    }
	gd::string pathForSFXFolder(int id) {
        auto path = MusicDownloadManager::pathForSFXFolder(id);
        log::debug("pathForSFXFolder: {}", path);
        return path;
    }
	gd::string pathForSong(int id) {
        auto path = MusicDownloadManager::pathForSong(id);
        log::debug("pathForSong: {}", path);
        return path;
    }
	gd::string pathForSongFolder(int id) {
        auto path = MusicDownloadManager::pathForSongFolder(id);
        log::debug("pathForSongFolder: {}", path);
        return path;
    }

};*/
