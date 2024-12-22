#include <Geode/Geode.hpp>


#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/CustomSongWidget.hpp>
#include <Geode/modify/CreatorLayer.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/ui/BasedButtonSprite.hpp>
//#include <Geode/loader/SettingEvent.hpp>
#include <filesystem>
#include <Geode/modify/MusicDownloadManager.hpp>
#include "blacklisted_ids.hpp"

using namespace geode::prelude;

class $modify(Button, LevelInfoLayer) {

    struct Fields {
        CCMenuItemSpriteExtra* m_trashButton = nullptr;
        CCMenuItemSpriteExtra* m_settingsButton = nullptr;
    };

    void getAndDeleteAudio(bool sfx, std::string songIDs, std::string sfxIDs);
    void deleteAudio();
    void openSettings(CCObject* obj);
    void onPlay(CCObject* sender);
    void button(CCObject* obj);
    bool init(GJGameLevel* level, bool challenge);
    void onBack(CCObject* obj);
    void levelDownloadFinished(GJGameLevel* level);

};

bool deleteSongs = false;
bool deleteSFX = false;
int songCount = 0;
bool isPlaying = false;

class trashPopup : public geode::Popup<Button*> {
    Button* layer = nullptr;
    bool song = false;
    bool sfx = false;
    bool autoSong = (Mod::get()->getSettingValue<bool>("auto-select-songs"));
    bool autoSFX = (Mod::get()->getSettingValue<bool>("auto-select-sfx"));
    CCMenuItemToggler* songToggle;
    CCMenuItemToggler* sfxToggle;
    protected:
        bool setup(Button* layer) override {
            this->layer = layer;
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
                static_cast<Button*>(layer)->m_fields->m_settingsButton = CCMenuItemSpriteExtra::create(settingsSprite, this, menu_selector(trashPopup::openSettings));
                popupMenu->addChild(static_cast<Button*>(layer)->m_fields->m_settingsButton);
                static_cast<Button*>(layer)->m_fields->m_settingsButton->setPosition({-146, 77});
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
        layer->deleteAudio();
        this->keyBackClicked();
    }

    void openSettings(CCObject* obj) {
        geode::openSettingsPopup(Mod::get());
    }

    public:
        static trashPopup* create(Button* layer) {
            auto ret = new trashPopup;
            if (ret->initAnchored(350, 213, layer, "square01_001.png", CCRectZero)) {
                ret->autorelease();
                return ret;
            }
            delete ret;
            return nullptr;
        }
};

    void Button::getAndDeleteAudio(bool sfx, std::string songIDs, std::string sfxIDs) {
        if (!sfx) songCount = 0;
        if (!sfx && songIDs == "" && sfxIDs == "") { 
            m_fields->m_trashButton->setVisible(false);
            return m_songWidget->deleteSong();
        }
        else if (!sfx && (songIDs == "" || !deleteSongs)) return m_fields->m_trashButton->setVisible(false); 
        else if (sfx && (sfxIDs == "" || !deleteSFX)) return m_fields->m_trashButton->setVisible(false); 
        std::string audioIDs = (sfx) ? sfxIDs : songIDs;
        std::stringstream ss(audioIDs);
        std::vector<std::string> tokens;
        std::string token;
        while (std::getline(ss, token, ',')) {
        tokens.push_back(token);
        }
        for (int i = 0; i < tokens.size(); i++) {
            std::string filename;
            if (blacklistedIDs.contains(std::stoi(tokens[i]))) continue;
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

    void Button::deleteAudio() {
        m_fields->m_trashButton->setVisible(false);
        std::string songIDs = std::string(m_level->m_songIDs);
        std::string sfxIDs = std::string(m_level->m_sfxIDs);
        if (songIDs == "" && sfxIDs == "")
            return m_songWidget->deleteSong();

        if (isPlaying && deleteSongs) {
            FMODAudioEngine::sharedEngine()->stopAllMusic(true);
            isPlaying = false;
            Loader::get()->queueInMainThread([this] {
                deleteAudio();
            });
            return;
        }

        getAndDeleteAudio(false, songIDs, sfxIDs);
        getAndDeleteAudio(true, songIDs, sfxIDs);

        auto newInfoLayer = LevelInfoLayer::create(m_level, false);
        removeFromParentAndCleanup(true);
        CCDirector::sharedDirector()->getRunningScene()->addChild(newInfoLayer);
        
        bool songs = (newInfoLayer->m_level->m_songIDs != "" && deleteSongs);
        bool sfx = (newInfoLayer->m_level->m_sfxIDs != "" && deleteSFX);
        bool all = (songs && sfx);
        bool none = (!songs && !sfx);
        bool robtopSFX = (newInfoLayer->m_songWidget->m_isRobtopSong && newInfoLayer->m_level->m_songIDs != "");
        std::string errorText = (all) ? "Songs & SFX Deleted" : ((songs) ? (songCount == 1) ? "Song Deleted" : "Songs Deleted" : ((sfx || robtopSFX) ? "SFX Deleted" : ""));

        newInfoLayer->m_songWidget->m_errorLabel->setColor(ccc3(255, 100, 0));
        newInfoLayer->m_songWidget->m_errorLabel->setString(errorText.c_str());
        newInfoLayer->m_songWidget->m_errorLabel->setVisible(true);
        newInfoLayer->m_songWidget->m_errorLabel->setScale(0.4f);

        deleteSongs = false;
        deleteSFX = false;
        songCount = 0;
    }
    
    void Button::openSettings(CCObject* obj) {
        geode::openSettingsPopup(Mod::get());
    }

    void Button::onPlay(CCObject* sender) {
        LevelInfoLayer::onPlay(sender);
        isPlaying = false;
    }

    void Button::button(CCObject* obj) {
        //if (m_songWidget->m_isRobtopSong) return;
        if (std::string(this->m_level->m_levelString) == "") return;
        std::string popupText = "Do you want to <cr>delete</c> ";
        if (m_songWidget->m_isRobtopSong) {
            popupText += "all SFX?";
            deleteSFX = true;
        }
        else {
            popupText += (std::string(this->m_level->m_songIDs) == "") ? "this song" : "all songs";
            popupText += (std::string(this->m_level->m_sfxIDs) != "") ? " and SFX?" : "?"; 
        }
            
        if ((!m_songWidget->m_isRobtopSong && std::string(this->m_level->m_sfxIDs) != "") || (std::string(this->m_level->m_songIDs) != "")) {
            auto popup = trashPopup::create(static_cast<Button*>(this));
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
                        deleteAudio();
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
            m_fields->m_settingsButton = CCMenuItemSpriteExtra::create(settingsSprite, this, menu_selector(Button::openSettings));
            settingsSprite->setScale(0.75);
            auto settingsMenu = CCMenu::create();
            CCArray* children = CCDirector::sharedDirector()->getRunningScene()->getChildren();
            auto popup = static_cast<FLAlertLayer*>(children->lastObject());
            popup->m_buttonMenu->addChild(m_fields->m_settingsButton);
            m_fields->m_settingsButton->setPosition({-147, 81});
            m_fields->m_settingsButton->setZOrder(1);	
        }
    }

    bool Button::init(GJGameLevel* level, bool challenge) {
        if (!LevelInfoLayer::init(level, challenge)) return false;

        auto sprite = CCSprite::createWithSpriteFrameName("GJ_trashBtn_001.png");
        sprite->setScale(0.8);
        m_fields->m_trashButton = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(Button::button));
        m_fields->m_trashButton->setPosition({128, -137});
        m_fields->m_trashButton->setID("trash-button"_spr);
    
        auto trashMenu = CCMenu::create();
        trashMenu->setID("trash-menu"_spr);
        this->addChild(trashMenu);
        trashMenu->addChild(m_fields->m_trashButton);
        m_fields->m_trashButton->setVisible(false);

        if (!m_songWidget->m_downloadBtn->isVisible() && !m_songWidget->m_cancelDownloadBtn->isVisible()) {
            m_fields->m_trashButton->setVisible(true);
        }	
        if ((m_songWidget->m_isRobtopSong && m_level->m_sfxIDs == "")) m_fields->m_trashButton->setVisible(false);

        if (auto robtopButton = this->m_songWidget->m_deleteBtn) {
            robtopButton->setVisible(false);
        }

        return true;
    }

    void Button::onBack(CCObject* obj) {
        LevelInfoLayer::onBack(obj);
        songCount = 0;
    }

    void Button::levelDownloadFinished(GJGameLevel* level) {
        LevelInfoLayer::levelDownloadFinished(level);
        if (m_fields->m_trashButton) m_fields->m_trashButton->setVisible(!m_songWidget->m_downloadBtn->isVisible() && !m_songWidget->m_cancelDownloadBtn->isVisible());
        if (!m_songWidget) return;
        if (m_songWidget->m_isRobtopSong && level->m_sfxIDs == "") m_fields->m_trashButton->setVisible(false);
    }

class $modify (CustomSongWidget) {
    virtual void downloadSongFinished(int i) {
        CustomSongWidget::downloadSongFinished(i);
        CCScene* scene = CCDirector::sharedDirector()->getRunningScene();
        LevelInfoLayer* layer = scene->getChildByType<LevelInfoLayer>(0);
        if (!layer) return;

        if (this == layer->m_songWidget && static_cast<Button*>(layer)->m_fields->m_trashButton && ((m_downloadBtn->isVisible() && !m_cancelDownloadBtn->isVisible()) || std::string(m_errorLabel->getString()) == "Download complete.")) 
            static_cast<Button*>(layer)->m_fields->m_trashButton->setVisible(true);
    }

    virtual void downloadAssetFinished(int i, GJAssetType assetType) {
        CustomSongWidget::downloadAssetFinished(i, assetType);
        CCScene* scene = CCDirector::sharedDirector()->getRunningScene();
        LevelInfoLayer* layer = scene->getChildByType<LevelInfoLayer>(0);
        if (!layer) return;

        if (assetType == GJAssetType::SFX && this == layer->m_songWidget && static_cast<Button*>(layer)->m_fields->m_trashButton && ((!m_downloadBtn->isVisible() && !m_cancelDownloadBtn->isVisible()) || std::string(m_errorLabel->getString()) == "Download complete.")) 
            static_cast<Button*>(layer)->m_fields->m_trashButton->setVisible(true);
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

$execute {
    geode::listenForSettingChanges("hide-settings-button", +[](bool  value) {
        CCScene* scene = CCDirector::sharedDirector()->getRunningScene();
        LevelInfoLayer* layer = scene->getChildByType<LevelInfoLayer>(0);
        if (!layer) return;

        if (static_cast<Button*>(layer)->m_fields->m_settingsButton)
            static_cast<Button*>(layer)->m_fields->m_settingsButton->setVisible((!value));
    });
};
