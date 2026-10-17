#include <QApplication>
#include <QLocale>
#include <QFileInfo>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QCheckBox>
#include <QIcon>
#include <QPixmap>
#include <QTimer>
#include <QDir>
#include <QSettings>
#include <QMap>
#include <QSignalBlocker>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QThread>
#include <stdexcept>

// Follow the first system UI language. No saved override or language control.
static bool isTurkish() {
    const auto languages = QLocale::system().uiLanguages();
    return !languages.isEmpty() && QLocale(languages.first()).language() == QLocale::Turkish;
}
static QString text(const char *en, const char *tr) {
    return QString::fromUtf8(isTurkish() ? tr : en);
}
static std::runtime_error errorText(const char *en, const char *tr) {
    return std::runtime_error(text(en, tr).toUtf8().constData());
}
static QString percentage(int value) {
    return isTurkish() ? QString("%%1").arg(value) : QString("%1%").arg(value);
}

static QString pulse(const QStringList &args) {
    QProcess p; p.start("/usr/bin/pactl", args);
    if (!p.waitForFinished(5000)) { p.kill(); p.waitForFinished(); throw errorText("The audio service did not respond.", "Ses hizmeti yanıt vermedi."); }
    if (p.exitCode()) throw std::runtime_error(p.readAllStandardError().trimmed().constData());
    return QString::fromUtf8(p.readAllStandardOutput()).trimmed();
}
static QJsonArray devices() { return QJsonDocument::fromJson(pulse({"-f","json","list","sinks"}).toUtf8()).array(); }
static QJsonArray streams() { return QJsonDocument::fromJson(pulse({"-f","json","list","sink-inputs"}).toUtf8()).array(); }
static QString deviceName(const QJsonObject &d) {
    const auto n=d["name"].toString();
    if(n.contains("Logitech_G432")) return "Logitech G432";
    if(n.contains("hdmi")) return text("Monitor / TV", "Monitör / TV");
    return d["description"].toString();
}
static QString deviceDetail(const QJsonObject &d) {
    const auto n=d["name"].toString();
    if(n.contains("Logitech_G432")) return text("USB headset", "USB kulaklık");
    if(n.contains("hdmi")) return text("HDMI audio output", "HDMI ses çıkışı");
    return text("Audio output", "Ses çıkışı");
}
static void syncPanelSelection(const QString &name) {
    const auto panelPath = QDir::homePath()+"/.config/lxqt/panel.conf";
    if (!QFileInfo::exists(panelPath)) return;
    // LXQt PulseAudioEngine sorts its device list by node name.
    QStringList names;for(const auto&v:devices())names<<v.toObject()["name"].toString();names.sort();
    int index=names.indexOf(name);if(index<0)return;
    QSettings settings(panelPath,QSettings::IniFormat);
    if(settings.value("volume/device",-1).toInt()!=index) {
        settings.setValue("volume/device",index);settings.sync();
    }
}
static void selectOutput(const QString &name) {
    bool exists=false;
    for(const auto &v:devices()) if(v.toObject()["name"].toString()==name) exists=true;
    if(!exists) throw errorText("This audio device was disconnected.", "Bu ses aygıtının bağlantısı kesildi.");
    pulse({"set-default-sink",name});
    for(const auto &v:streams()) {
        int index=v.toObject()["index"].toInt();
        try { pulse({"move-sink-input",QString::number(index),name}); }
        catch(...) {
            for(const auto &s:streams()) if(s.toObject()["index"].toInt()==index) throw;
        }
    }
    if(pulse({"get-default-sink"})!=name) throw errorText("Could not change the audio output.", "Ses çıkışı değiştirilemedi.");
    syncPanelSelection(name);
}

class Picker:public QDialog {
    QVBoxLayout *deviceBox;
    QLabel *status,*volumeText,*playing,*error;
    QSlider *slider;
    QCheckBox *mute;
    QMap<QString,QPushButton*> buttons;
    QStringList names;
    QJsonObject current;
    void fail(const std::exception &e) { error->setText(QString::fromUtf8(e.what())); error->show(); }
    void setVolume() {
        if(current.isEmpty())return;
        try { pulse({"set-sink-volume",current["name"].toString(),QString::number(slider->value())+"%"}); }
        catch(const std::exception&e){fail(e);}
    }
public:
    Picker() {
        setWindowTitle(text("Audio output", "Ses çıkışı")); setWindowIcon(QIcon::fromTheme("audio-headphones")); setMinimumWidth(440);
        setStyleSheet(R"(
QDialog { background:#202630; color:#f4f6fb; }
QLabel,QCheckBox { color:#f4f6fb; font-size:14px; }
QLabel#title { font-size:23px; font-weight:600; }
QLabel#detail { color:#bbc6d6; font-size:12px; }
QPushButton { background:#2b3340; color:#f4f6fb; border:1px solid #465164; border-radius:9px; padding:12px; font-size:14px; text-align:left; }
QPushButton:hover { background:#364152; }
QPushButton:checked { border:2px solid #69aaff; background:#263d59; }
QPushButton#advanced { padding:8px; font-size:12px; text-align:center; }
QSlider::groove:horizontal { height:5px; background:#596477; border-radius:2px; }
QSlider::sub-page:horizontal { background:#69aaff; border-radius:2px; }
QSlider::handle:horizontal { background:#e4efff; width:15px; margin:-5px 0; border-radius:7px; }
)");
        auto outer=new QVBoxLayout(this); outer->setContentsMargins(22,20,22,18); outer->setSpacing(13);
        auto title=new QLabel(text("Audio output", "Ses çıkışı")); title->setObjectName("title"); outer->addWidget(title);
        status=new QLabel; status->setWordWrap(true); outer->addWidget(status);
        deviceBox=new QVBoxLayout; deviceBox->setSpacing(9); outer->addLayout(deviceBox);
        auto row=new QHBoxLayout; row->addWidget(new QLabel(text("Volume", "Ses düzeyi"))); row->addStretch();
        volumeText=new QLabel; row->addWidget(volumeText); outer->addLayout(row);
        slider=new QSlider(Qt::Horizontal); slider->setRange(0,100); outer->addWidget(slider);
        connect(slider,&QSlider::valueChanged,this,[this](int v){volumeText->setText(percentage(v));if(!slider->isSliderDown())setVolume();});
        connect(slider,&QSlider::sliderReleased,this,[this]{setVolume();});
        mute=new QCheckBox(text("Mute", "Sessize al")); outer->addWidget(mute);
        connect(mute,&QCheckBox::toggled,this,[this](bool checked){if(!current.isEmpty())try{pulse({"set-sink-mute",current["name"].toString(),checked?"1":"0"});}catch(const std::exception&e){fail(e);}});
        playing=new QLabel; playing->setWordWrap(true); playing->setObjectName("detail"); outer->addWidget(playing);
        error=new QLabel; error->setWordWrap(true); error->setStyleSheet("color:#ffbeaa;font-size:12px;");error->hide();outer->addWidget(error);
        auto advanced=new QPushButton(text("Advanced audio settings", "Gelişmiş ses ayarları"));advanced->setObjectName("advanced");outer->addWidget(advanced);
        connect(advanced,&QPushButton::clicked,this,[]{QProcess::startDetached("/usr/bin/pavucontrol-qt",{});});
        auto timer=new QTimer(this);connect(timer,&QTimer::timeout,this,[this]{refresh();});timer->start(2000);refresh();
    }
    void refresh() {
        try {
            auto ds=devices();auto def=pulse({"get-default-sink"});auto ss=streams();QStringList next;
            for(const auto&v:ds)next<<v.toObject()["name"].toString();
            if(next!=names) {
                while(auto item=deviceBox->takeAt(0)){delete item->widget();delete item;}
                buttons.clear();names=next;
                for(const auto&v:ds) {
                    auto d=v.toObject();auto n=d["name"].toString();auto b=new QPushButton;
                    b->setCheckable(true);b->setAutoDefault(false);b->setMinimumHeight(68);
                    b->setIcon(QIcon::fromTheme(n.contains("hdmi")?"video-display":"audio-headphones"));
                    connect(b,&QPushButton::clicked,this,[this,n]{try{selectOutput(n);error->hide();refresh();}catch(const std::exception&e){fail(e);}});
                    deviceBox->addWidget(b);buttons[n]=b;
                }
            }
            current={};QMap<int,QString> routes;
            for(const auto&v:ds) {
                auto d=v.toObject();auto n=d["name"].toString();bool selected=n==def;
                buttons[n]->setChecked(selected);
                buttons[n]->setText((selected?QString("✓  "):QString())+deviceName(d)+"\n"+deviceDetail(d)+(selected?text("  ·  Selected", "  ·  Seçili"):QString()));
                routes[d["index"].toInt()]=deviceName(d);if(selected)current=d;
            }
            slider->setEnabled(!current.isEmpty());mute->setEnabled(!current.isEmpty());
            if(!current.isEmpty()) {
                status->setText(text("Selected output: ", "Seçili çıkış: ")+deviceName(current));
                if(!slider->isSliderDown()) {
                    double sum=0;auto vol=current["volume"].toObject();
                    for(auto it=vol.begin();it!=vol.end();++it)sum+=it.value().toObject()["value"].toDouble();
                    int value=vol.isEmpty()?0:qRound(sum/vol.size()*100/65536);
                    QSignalBlocker block(slider);slider->setValue(qBound(0,value,100));volumeText->setText(percentage(slider->value()));
                }
                QSignalBlocker block(mute);mute->setChecked(current["mute"].toBool());
            }else status->setText(text("No audio output is available.", "Kullanılabilir bir ses çıkışı yok."));
            QStringList active;
            for(const auto&s:ss){auto n=routes.value(s.toObject()["sink"].toInt(),text("Other device", "Diğer aygıt"));if(!active.contains(n))active<<n;}
            playing->setText(active.isEmpty()?text("No application is playing audio.", "Şu anda ses çalan uygulama yok."):text("Playing through: ", "Çalan ses: ")+active.join(", "));
        }catch(const std::exception&e){fail(e);}
    }
};

static QJsonObject selfTest() {
    QString original=pulse({"get-default-sink"});auto before=devices();
    if(!streams().isEmpty())throw errorText("Stop other audio playback before running the routing test.", "Yönlendirme testi için çalan ses olmamalı.");
    QProcess sound;sound.setStandardInputFile("/dev/zero");sound.setStandardOutputFile(QProcess::nullDevice());sound.setStandardErrorFile(QProcess::nullDevice());
    sound.start("/usr/bin/pacat",{"--playback","--raw","--format=s16le","--rate=48000","--channels=2","--client-name=LinuxAudioOutputSelectorTest","--stream-name=SilentTest"});
    if(!sound.waitForStarted())throw errorText("Could not start the silent test stream.", "Sessiz test akışı başlatılamadı.");
    QJsonArray tested;
    try {
        QThread::msleep(500);
        for(const auto&v:devices()) {
            auto d=v.toObject();selectOutput(d["name"].toString());auto playing=streams();
            if(playing.isEmpty())throw errorText("The test stream was not found.", "Test akışı bulunamadı.");
            for(const auto&s:playing)if(s.toObject()["sink"].toInt()!=d["index"].toInt())throw errorText("The stream was not moved to the selected output.", "Akış doğru çıkışa taşınmadı.");
            tested.append(deviceName(d));
        }
        sound.terminate();sound.waitForFinished(3000);selectOutput(original);
    }catch(...){sound.kill();sound.waitForFinished();try{selectOutput(original);}catch(...){}throw;}
    for(const auto&v:before)for(const auto&a:devices())if(v.toObject()["name"]==a.toObject()["name"])
        if(v.toObject()["volume"]!=a.toObject()["volume"]||v.toObject()["mute"]!=a.toObject()["mute"])throw errorText("Volume or mute state changed unexpectedly.", "Ses düzeyi beklenmedik biçimde değişti.");
    return {{"tested_outputs",tested},{"default_and_stream_routing_verified",true},{"original_default_restored",pulse({"get-default-sink"})==original},{"volumes_and_mutes_unchanged",true},{"test_audio","Digital silence only"}};
}

int main(int argc,char**argv) {
    // Keep locale verification independent of the display and audio server.
    for (int i=1; i<argc; ++i) if (QString::fromLocal8Bit(argv[i])=="--locale-info") {
        QCoreApplication app(argc,argv);
        QJsonObject info{{"language",isTurkish()?"tr":"en"},
                         {"system_ui_languages",QJsonArray::fromStringList(QLocale::system().uiLanguages())},
                         {"title",text("Audio output", "Ses çıkışı")},
                         {"percentage",percentage(40)}};
        fputs(QJsonDocument(info).toJson().constData(),stdout);return 0;
    }
    QApplication app(argc,argv);app.setApplicationName("linux-audio-output-selector");
    try {
        auto args=app.arguments();
        if(args.contains("--self-test")){auto r=selfTest();fputs(QJsonDocument(r).toJson().constData(),stdout);return 0;}
        if(args.contains("--configure-panel")) {
            if(!QFileInfo::exists(QDir::homePath()+"/.config/lxqt/panel.conf"))throw errorText("LXQt panel configuration was not found.", "LXQt panel yapılandırması bulunamadı.");
            QSettings s(QDir::homePath()+"/.config/lxqt/panel.conf",QSettings::IniFormat);
            s.setValue("volume/mixerCommand",QDir::homePath()+"/.local/bin/linux-audio-output-selector");s.sync();
            if(s.status()!=QSettings::NoError)throw errorText("Could not save panel settings.", "Panel ayarı kaydedilemedi.");syncPanelSelection(pulse({"get-default-sink"}));return 0;
        }
        Picker picker;picker.show();
        int snapshot=args.indexOf("--snapshot");
        if(snapshot>=0 && args.size()>snapshot+1)QTimer::singleShot(800,[&]{picker.grab().save(args[snapshot+1]);app.quit();});
        return app.exec();
    }catch(const std::exception&e){fprintf(stderr,"%s\n",e.what());return 1;}
}
