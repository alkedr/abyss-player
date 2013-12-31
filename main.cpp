#include <QtGui>
#include <QtWidgets>
#include <QtMultimedia>

#include <boost/filesystem.hpp>
#include <vector>
#include <string>
#include <memory>


namespace {

class FileSystemObject {
	const boost::filesystem::path path_;
public:
	FileSystemObject(const std::string & path) : path_(path) {}
	const boost::filesystem::path & path() const { return path_; }
};

class File : public FileSystemObject {
public:
	using FileSystemObject::FileSystemObject;
};

class Directory : public FileSystemObject {
	std::vector<File> files_;
public:
	using FileSystemObject::FileSystemObject;
};


class Playlist {
	std::vector<std::unique_ptr<FileSystemObject>> fsObjects;
	std::vector<File> expanded;
public:
	void addFile(const std::string & path) {
		fsObjects.emplace_back(new File(path));
	}
	void addDir(const std::string & path) {
		fsObjects.emplace_back(new Directory(path));
	}
};



using PathList = std::vector<boost::filesystem::path>;   // can include files and directories


class PlaylistModel : public QAbstractTableModel {
public:
	PlaylistModel(QMediaPlaylist * playlist) : playlist_(playlist) {
	}

	int columnCount(const QModelIndex & parent) const override {
		if (parent.isValid()) return 0;
		return 2;
	}

	QVariant headerData(int section, Qt::Orientation orientation, int role) const override {
		if (role != Qt::DisplayRole) return QVariant();

		if (orientation == Qt::Horizontal) {
			switch (section) {
				case 0: return QString("file name");
				case 1: return QString("time");
			}
		} else {
			return QString::number(section+1);
		}
		return QVariant();
	}

	int rowCount(const QModelIndex & parent) const override {
		if (parent.isValid()) return 0;
		return playlist_->mediaCount();
	}

	QVariant data(const QModelIndex & index, int role) const override {
		// TODO: colors
		if (role != Qt::DisplayRole) return QVariant();

		const auto & media = playlist_->media(index.row());
		switch (index.column()) {
			case 0: return media.canonicalUrl();
			default: return QVariant();
		}
	}

private:
	QMediaPlaylist * playlist_;
};


QString millisecondsToString(qint64 val) {
	uint8_t seconds = (uint8_t)((val / 1000) % 60);
	uint8_t minutes = (uint8_t)((val / 1000 / 60) % 60);
	uint16_t hours = (uint16_t)(val / 1000 / 60 / 60);

	QString res;
	if (hours > 0) res += QString::number(hours) + ":";
	if ((hours > 0) && (minutes < 10)) res += "0";
	res += QString::number(minutes) + ":";
	if (seconds < 10) res += "0";
	res += QString::number(seconds);
	return res;
}

class MySlider : public QSlider {
protected:
	void mousePressEvent(QMouseEvent * event) {
		QStyleOptionSlider opt;
		initStyleOption(&opt);
		QRect sr = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);

		if (event->button() == Qt::LeftButton && sr.contains(event->pos()) == false) {
			int newVal;
			if (orientation() == Qt::Vertical)
				newVal = minimum() + ((maximum()-minimum()) * (height()-event->y())) / height();
			else
				newVal = minimum() + ((maximum()-minimum()) * event->x()) / width();

			if (invertedAppearance() == true)
				setValue(maximum() - newVal);
			else
				setValue(newVal);

			event->accept();
		}
		QSlider::mousePressEvent(event);
	}
};


class ButtonsAndVolumeControlWidget : public QWidget {
public:
	ButtonsAndVolumeControlWidget(QMediaPlayer * player) {
		auto mainLayout = new QHBoxLayout(this);
			auto playPauseButton = createCheckableButton("media-playback-start", "p", mainLayout);
			auto stopButton = createCheckableButton("media-playback-stop", "s", mainLayout);
			auto prevButton = createButton("go-previous", "", mainLayout);
			auto nextButton = createButton("go-next", "", mainLayout);
			auto muteButton = createCheckableButton("audio-volume-muted", "m", mainLayout);
			auto repeatButton = createCheckableButton("media-playlist-repeat", "r", mainLayout);
			auto randomizeButton = createCheckableButton("media-playlist-shuffle", "Shift+r", mainLayout);
			auto volumeSlider = createSlider(mainLayout);

		connect(player, &QMediaPlayer::stateChanged,
			[playPauseButton, stopButton](QMediaPlayer::State newState) {
				if (newState == QMediaPlayer::StoppedState) {
					playPauseButton->setChecked(false);
					stopButton->setChecked(true);
				} else if (newState == QMediaPlayer::PlayingState) {
					playPauseButton->setChecked(true);
					stopButton->setChecked(false);
				} else if (newState == QMediaPlayer::PausedState) {
					playPauseButton->setChecked(false);
					stopButton->setChecked(false);
				}
			}
		);

		connect(player, &QMediaPlayer::volumeChanged,
			[volumeSlider](int newVolume) {
				volumeSlider->blockSignals(true);
				volumeSlider->setSliderPosition(newVolume);
				volumeSlider->blockSignals(false);
			}
		);

		connect(playPauseButton, &QPushButton::clicked,
			[player]() {
				// TODO check button state instead
				if (player->state() == QMediaPlayer::PlayingState) {
					player->pause();
				} else {
					player->play();
				}
			}
		);

		connect(stopButton, &QPushButton::clicked,
			[player]() {
				// TODO check button state
				player->stop();
			}
		);

		connect(prevButton, &QPushButton::clicked,
			[]() {
			}
		);

		connect(nextButton, &QPushButton::clicked,
			[]() {
			}
		);

		connect(muteButton, &QPushButton::clicked,
			[player, muteButton]() {
				player->setMuted(muteButton->isChecked());
			}
		);


		connect(volumeSlider, &MySlider::valueChanged,
			[player](int newValue) {
				player->setVolume(newValue);
			}
		);
	}

private:
	static QPushButton * createButton(const char* iconName, const char* keySequenceString, QLayout * layout) {
		auto res = new QPushButton(QIcon::fromTheme(iconName), iconName);
		res->setShortcut(QKeySequence(keySequenceString));
		layout->addWidget(res);
		return res;
	}

	static QPushButton * createCheckableButton(const char* iconName, const char* keySequenceString, QLayout * layout) {
		auto res = createButton(iconName, keySequenceString, layout);
		res->setCheckable(true);
		return res;
	}

	static QSlider * createSlider(QLayout * layout) {
		auto res = new MySlider;
		res->setOrientation(Qt::Horizontal);
		layout->addWidget(res);
		return res;
	}
};


class TimeControlWidget : public QWidget {
public:
	TimeControlWidget(QMediaPlayer * player) {
		auto mainLayout = new QHBoxLayout(this);
			auto timeSlider = new MySlider();
			auto timeLabel = new QLabel("0:00 / 0:00");

		mainLayout->addWidget(timeSlider);
		mainLayout->addWidget(timeLabel);

		timeSlider->setOrientation(Qt::Horizontal);
		timeSlider->setTracking(false);

		connect(timeSlider, &MySlider::valueChanged,
			[player](int newValue) {
				player->setPosition(newValue);
			}
		);

		connect(player, &QMediaPlayer::positionChanged,
			[timeSlider, timeLabel, player](qint64 newPosition) {
				timeSlider->blockSignals(true);
				timeSlider->setSliderPosition((qint32)newPosition);
				timeSlider->blockSignals(false);
				timeLabel->setText(millisecondsToString(newPosition) + " / " + millisecondsToString(player->duration()));
			}
		);

		connect(player, &QMediaPlayer::durationChanged,
			[timeSlider, timeLabel, player](qint64 newDuration) {
				timeSlider->setMaximum((qint32)newDuration);
				timeLabel->setText(millisecondsToString(player->position()) + " / " + millisecondsToString(newDuration));
			}
		);
	}
};

void addDirectoryToPlaylist(QMediaPlaylist * playlist, const char * dirName) {
	for (
		boost::filesystem::recursive_directory_iterator it(dirName);
		it != boost::filesystem::recursive_directory_iterator();
		it++
	) {
		const auto & path = it->path();
		if (!boost::filesystem::is_directory(path)) {
			playlist->addMedia(QUrl::fromLocalFile(path.c_str()));
		}
	}
}

QMediaPlaylist * loadPlaylist() {
	QMediaPlaylist * playlist = new QMediaPlaylist();
	addDirectoryToPlaylist(playlist, "/home/alkedr/music");
	return playlist;
}

class PlaylistWidget : public QTableView {
public:
	PlaylistWidget(QMediaPlayer * player) {
		QMediaPlaylist * playlist = loadPlaylist();
		setModel(new PlaylistModel(playlist));
		player->setPlaylist(playlist);

		connect(this, &QAbstractItemView::activated,
			[player](const QModelIndex & index) {
				if (index.isValid()) {
					player->playlist()->setCurrentIndex(index.row());
				}
			}
		);
	}
};


class MainWindow : public QWidget {
	QMediaPlayer player;

public:
	MainWindow() {
		auto mainLayout = new QVBoxLayout(this);
			mainLayout->addWidget(new ButtonsAndVolumeControlWidget(&player));
			mainLayout->addWidget(new TimeControlWidget(&player));
			mainLayout->addWidget(new PlaylistWidget(&player));

		player.setNotifyInterval(50);
		player.setVolume(50);
	}

	~MainWindow() {
		player.stop();
	}
};

}


int main(int argc, char ** argv) {
	QApplication app(argc, argv);
	(new MainWindow())->show();
	return app.exec();
}
