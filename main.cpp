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
	PlaylistModel(const QMediaPlaylist & playlist) : playlist_(playlist) {
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
		return playlist_.mediaCount();
	}

	QVariant data(const QModelIndex & index, int role) const override {
		if (role != Qt::DisplayRole) return QVariant();

		const auto & media = playlist_.media(index.row());
		switch (index.column()) {
			case 0: return media.canonicalUrl();
			default: return QVariant();
		}
	}

private:
	const QMediaPlaylist & playlist_;
};


std::string millisecondsToString(qint64 val) {
	uint8_t seconds = (uint8_t)((val / 1000) % 60);
	uint8_t minutes = (uint8_t)((val / 1000 / 60) % 60);
	uint16_t hours = (uint16_t)(val / 1000 / 60 / 60);

	std::string res;
	if (hours > 0) res += std::to_string(hours) + ":";
	if ((hours > 0) && (minutes < 10)) res += "0";
	res += std::to_string(minutes) + ":";
	if (seconds < 10) res += "0";
	res += std::to_string(seconds);
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

class MainWindow : public QMainWindow {
	QMediaPlayer player;

	QWidget mainWidget;
	QVBoxLayout mainLayout;
		QHBoxLayout firstLineLayout;
			QPushButton playPauseButton;
			QPushButton stopButton;
			QPushButton nextButton;
			QPushButton prevButton;
			QPushButton muteButton;
			QPushButton repeatButton;
			QPushButton randomizeButton;
			MySlider volumeSlider;
		QHBoxLayout secondLineLayout;
			MySlider timeSlider;
			QLabel timeLabel;
		QTableView playlistTable;
		PlaylistModel playlistModel;
		QMediaPlaylist playlist;

public:

	MainWindow()
 	: playPauseButton(QIcon::fromTheme("media-playback-start"), "")
	, stopButton(QIcon::fromTheme("media-playback-stop"), "")
	, nextButton(QIcon::fromTheme("go-next"), "")
	, prevButton(QIcon::fromTheme("go-previous"), "")
	, muteButton(QIcon::fromTheme("audio-volume-muted"), "")
	, repeatButton(QIcon::fromTheme("media-playlist-repeat"), "")
	, randomizeButton(QIcon::fromTheme("media-playlist-shuffle"), "")
	, timeLabel("0:00 / 0:00")
	, playlistModel(playlist)
	{
		playPauseButton.setCheckable(true);
		playPauseButton.setShortcut(QKeySequence("p"));
		stopButton.setCheckable(true);
		stopButton.setShortcut(QKeySequence("s"));
		muteButton.setCheckable(true);
		muteButton.setShortcut(QKeySequence("m"));
		repeatButton.setCheckable(true);
		repeatButton.setShortcut(QKeySequence("r"));
		randomizeButton.setCheckable(true);
		randomizeButton.setShortcut(QKeySequence("Shift+r"));

		volumeSlider.setOrientation(Qt::Horizontal);
		volumeSlider.setMaximum(100);
		timeSlider.setOrientation(Qt::Horizontal);
		timeSlider.setTracking(false);

		playlist.addMedia(QUrl::fromLocalFile("/home/alkedr/music/zero-project/zero-project - 02 - Gothic.ogg"));

		playlistTable.setModel(&playlistModel);

		setCentralWidget(&mainWidget);
		mainWidget.setLayout(&mainLayout);
			mainLayout.addLayout(&firstLineLayout, 0);
				firstLineLayout.addWidget(&playPauseButton);
				firstLineLayout.addWidget(&stopButton);
				firstLineLayout.addWidget(&nextButton);
				firstLineLayout.addWidget(&prevButton);
				firstLineLayout.addWidget(&muteButton);
				firstLineLayout.addWidget(&repeatButton);
				firstLineLayout.addWidget(&randomizeButton);
				firstLineLayout.addWidget(&volumeSlider, 1);
			mainLayout.addLayout(&secondLineLayout, 0);
				secondLineLayout.addWidget(&timeSlider);
				secondLineLayout.addWidget(&timeLabel);
			mainLayout.addWidget(&playlistTable, 1);


		connect(&player, &QMediaPlayer::stateChanged,
			[this](QMediaPlayer::State newState) {
				if (newState == QMediaPlayer::StoppedState) {
					playPauseButton.setChecked(false);
					stopButton.setChecked(true);
				} else if (newState == QMediaPlayer::PlayingState) {
					playPauseButton.setChecked(true);
					stopButton.setChecked(false);
				} else if (newState == QMediaPlayer::PausedState) {
					playPauseButton.setChecked(false);
					stopButton.setChecked(false);
				}
			}
		);

		connect(&player, &QMediaPlayer::positionChanged,
			[this](qint64 newPosition) {
				timeSlider.blockSignals(true);
				timeSlider.setSliderPosition((qint32)newPosition);
				timeSlider.blockSignals(false);
				timeLabel.setText(QString::fromStdString(millisecondsToString(newPosition) + " / " + millisecondsToString(player.duration())));
			}
		);

		connect(&player, &QMediaPlayer::durationChanged,
			[this](qint64 newDuration) {
				timeSlider.setMaximum((qint32)newDuration);
				timeLabel.setText(QString::fromStdString(millisecondsToString(player.position()) + " / " + millisecondsToString(newDuration)));
			}
		);

		connect(&player, &QMediaPlayer::volumeChanged,
			[this](int newVolume) {
				volumeSlider.blockSignals(true);
				volumeSlider.setSliderPosition(newVolume);
				volumeSlider.blockSignals(false);
			}
		);

		connect(&player, &QMediaPlayer::setPlaylist,
			[this](QMediaPlaylist * newPlaylist) {
			}
		);


		connect(&playPauseButton, &QPushButton::clicked,
			[this]() {
				if (player.state() == QMediaPlayer::PlayingState) {
					player.pause();
				} else if (player.state() == QMediaPlayer::PausedState) {
					player.play();
				}
			}
		);

		connect(&stopButton, &QPushButton::clicked,
			[this]() {
				player.stop();
			}
		);

		connect(&prevButton, &QPushButton::clicked,
			[this]() {
			}
		);

		connect(&nextButton, &QPushButton::clicked,
			[this]() {
			}
		);

		connect(&muteButton, &QPushButton::clicked,
			[this]() {
				player.setMuted(muteButton.isChecked());
			}
		);


		connect(&volumeSlider, &MySlider::valueChanged,
			[this](int newValue) {
				player.setVolume(newValue);
			}
		);

		connect(&timeSlider, &MySlider::valueChanged,
			[this](int newValue) {
				player.setPosition(newValue);
			}
		);


		connect(&playlistTable, &QAbstractItemView::activated,
			[this](const QModelIndex & index) {
				if (index.isValid()) {
					player.playlist()->setCurrentIndex(index.row());
				}
			}
		);

		player.setPlaylist(&playlist);

		player.setNotifyInterval(50);
		player.setVolume(50);
		player.play();
	}

	~MainWindow() {
		player.stop();
	}

};

}

int main(int argc, char ** argv) {
	QApplication app(argc, argv);

	MainWindow mainWindow;
	mainWindow.show();

	return app.exec();
}

