#include <Geode/Geode.hpp>

using namespace geode::prelude;

#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/CustomSongWidget.hpp>

CustomSongWidget* songWidget = nullptr;
CCMenuItemSpriteExtra* trashButton = nullptr;
CCNode* downloadButton = nullptr;
CCNode* cancelButton = nullptr;
#ifdef GEODE_IS_ANDROID
bool android = true;
#else
bool android = false;
#endif

class $modify(Button, LevelInfoLayer) {

	struct Fields {
		bool hasInfoBtn = false;
	};

	void onDelete() {
		trashButton->setVisible(false);
		if ((this->m_level->m_songIDs) == "")
			return songWidget->deleteSong();
		deleteSongs(this->m_level);
		auto newInfoLayer = LevelInfoLayer::create(this->m_level, false);
		this->removeFromParentAndCleanup(true);
		CCDirector::sharedDirector()->getRunningScene()->addChild(newInfoLayer);

		songWidget->m_errorLabel->setColor(ccc3(255, 100, 0));
		std::string errorText = (!Mod::get()->getSettingValue<bool>("delete-sfx") && std::string(this->m_level->m_sfxIDs) == "") ? "Songs Deleted" : "Songs & SFX Deleted";
		songWidget->m_errorLabel->setString(errorText.c_str());
		songWidget->m_errorLabel->setVisible(true);
		songWidget->m_errorLabel->setScale(0.4f);
	}

	void button(CCObject* obj) {
		if (std::string(this->m_level->m_levelString) == "") return;
		std::string popupText = "Do you want to <cr>delete</c> ";

		popupText += std::string(this->m_level->m_songIDs) == "" ? "this song" : "all songs";
		popupText += (Mod::get()->getSettingValue<bool>("delete-sfx") && std::string(this->m_level->m_sfxIDs) != "") ? " and SFX?" : "?" ;
			
		geode::createQuickPopup(
			"Delete Audio", 
			popupText,
			"Cancel", "Delete",
			[this](auto, bool btn2) {
				if (btn2)  {
					this->onDelete();
				}
			}
		);
	}

	void deleteSongs(GJGameLevel* level) {
			bool sfx;
			std::string songIDs = std::string(level->m_songIDs);
			std::string sfxIDs = std::string(level->m_sfxIDs);
			getAndDeleteAudio(level, false, songIDs, sfxIDs);
			getAndDeleteAudio(level, true, songIDs, sfxIDs);
	}

	void getAndDeleteAudio(GJGameLevel* level, bool sfx, std::string songIDs, std::string sfxIDs) {
		if (!sfx && songIDs == "" && (sfxIDs == "" || !Mod::get()->getSettingValue<bool>("delete-sfx"))) {
			trashButton->setVisible(false);
			return songWidget->deleteSong();
		}
		else if (sfx && (sfxIDs == "" || !Mod::get()->getSettingValue<bool>("delete-sfx"))) return trashButton->setVisible(false);
		std::string audioIDs = (sfx) ? sfxIDs : songIDs;
		std::stringstream ss(audioIDs);
   		std::vector<std::string> tokens;
   		std::string token;
   		while (std::getline(ss, token, ',')) {
    	tokens.push_back(token);
   		}
		std::string saveDir = dirs::getSaveDir().string();
		for (int i = 0; i < tokens.size(); i++) {
			auto filename = (android ? ((sfx) ? (saveDir + "/" + "s" + tokens[i] + ".ogg") : (saveDir + "/" + tokens[i])) : ((sfx) ? (saveDir + "\\" + "s" + tokens[i] + ".ogg") : (saveDir + "\\" + tokens[i])));
			if (sfx) {
				std::filesystem::remove(filename);	
				continue;
			}
			if (!std::filesystem::remove(filename + ".mp3"))
			std::filesystem::remove(filename + ".ogg");
		}
	}

	bool init(GJGameLevel* level, bool challenge) {
		if (!LevelInfoLayer::init(level, challenge)) return false;

		songWidget = static_cast<CustomSongWidget*>(this->getChildByID("custom-songs-widget"));
		if (songWidget->m_isRobtopSong) return true;
		m_fields->hasInfoBtn = songWidget->m_infoBtn->isVisible();

		downloadButton = songWidget->getChildByID("buttons-menu")->getChildByID("download-button");
		cancelButton = songWidget->getChildByID("buttons-menu")->getChildByID("cancel-button");

		auto sprite = CCSprite::createWithSpriteFrameName("GJ_resetBtn_001.png");
		sprite->setScale(1.3);
		trashButton = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(Button::button));
		trashButton->setPosition({128, -136});
		trashButton->setID("trash-button");
	
		auto trashMenu = CCMenu::create();
		trashMenu->setID("trash-menu");
		this->addChild(trashMenu);
		trashMenu->addChild(trashButton);
		trashButton->setVisible(false);

		if (!downloadButton->isVisible() && !cancelButton->isVisible()) {
			trashButton->setVisible(true);
		}	

		return true;
	}

	void onBack(CCObject* obj) {
		LevelInfoLayer::onBack(obj);
		songWidget = nullptr;
		trashButton = nullptr;
		downloadButton = nullptr;
		cancelButton = nullptr;
	}

	void levelDownloadFinished(GJGameLevel* level) {
		LevelInfoLayer::levelDownloadFinished(level);
		if (trashButton) trashButton->setVisible(!downloadButton->isVisible() && !cancelButton->isVisible());
	}
};

class $modify (CustomSongWidget) {
	virtual void downloadSongFinished(int i) {
		CustomSongWidget::downloadSongFinished(i);
		if (this == songWidget && trashButton && ((!downloadButton->isVisible() && !cancelButton->isVisible()) || std::string(this->m_errorLabel->getString()) == "Download complete.")) 
			trashButton->setVisible(true);
	}

	virtual void downloadSFXFinished(int i) {
		CustomSongWidget::downloadSFXFinished(i);
		if (this == songWidget && trashButton && ((!downloadButton->isVisible() && !cancelButton->isVisible()) || std::string(this->m_errorLabel->getString()) == "Download complete.")) 
			trashButton->setVisible(true);
	}
};