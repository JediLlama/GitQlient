#include "GravatarImage.h"

#include <QCryptographicHash>
#include <QNetworkReply>
#include <QTimerEvent>

static const int delayedReloadTimeout = 500;

GravatarImage::GravatarImage(QWidget* parent)
:  QLabel(parent)
{
   if (!_namStorage.hasLocalData())
      _namStorage.setLocalData(new QNetworkAccessManager);

   setScaledContents(true);
   setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

QSize GravatarImage::sizeHint() const
{
   return {64, 64};
}

void GravatarImage::setEmailAddress(const QString& emailAddress)
{
   if (emailAddress != _emailAddress) {
      _emailAddress = emailAddress;
      emit emailAddressChanged();
      _emailAddressHash.clear();
      delayedReload();
   }
}

QString GravatarImage::emailAddress() const
{
   return _emailAddress;
}

void GravatarImage::setDefaultImage(DefaultImage defaultImage)
{
   if (defaultImage != _defaultImage) {
      _defaultImage = defaultImage;
      delayedReload();
   }
}

GravatarImage::DefaultImage GravatarImage::defaultImage() const
{
   return _defaultImage;
}

void GravatarImage::setRating(Rating rating)
{
   if (rating != _rating) {
      rating = _rating;
      delayedReload();
   }
}

GravatarImage::Rating GravatarImage::rating() const
{
   return _rating;
}

bool GravatarImage::hasHeightForWidth() const
{
   return true;
}

int GravatarImage::heightForWidth(int width) const
{
   return width;
}

void GravatarImage::resizeEvent(QResizeEvent* event)
{
   QLabel::resizeEvent(event);
   delayedReload();
}

void GravatarImage::delayedReload()
{
   if (_reloadTimer != 0)
      killTimer(_reloadTimer);

   _reloadTimer = startTimer(delayedReloadTimeout);
}

void GravatarImage::timerEvent(QTimerEvent* event)
{
   if (event->timerId() == _reloadTimer) {
      killTimer(_reloadTimer);
      _reloadTimer = 0;
      reload();
   }
}

static std::array<const char*, 8> defaultImageMap = {
   "",
   "mp",
   "identicon",
   "monsterid",
   "wavatar",
   "retro",
   "robohash",
   "blank"
};

static std::array<const char*, 5> ratingMap = {
   "",
   "g",
   "pg",
   "r",
   "x"
};

void GravatarImage::reload()
{
   if (_emailAddressHash.isEmpty()) {
      QCryptographicHash hash(QCryptographicHash::Md5);
      hash.addData(_emailAddress.trimmed().toLower().toLocal8Bit());
      _emailAddressHash = hash.result().toHex();
   }

   QStringList queryItems;
   queryItems += QString("s=%1").arg(width());

   try {
      QString defaultImage = defaultImageMap.at(static_cast<std::size_t>(_defaultImage));

      if (!defaultImage.isEmpty())
         queryItems += QString("d=%1").arg(defaultImage);
   } catch (const std::out_of_range& e) {
      // No default image
   }

   try {
      QString rating = ratingMap.at(static_cast<std::size_t>(_rating));

      if (!rating.isEmpty())
         queryItems += QString("r=%1").arg(rating);
   } catch (const std::out_of_range& e) {
      // No rating
   }

   QUrl url(QString("https://gravatar.com/avatar/%1.png?").arg(_emailAddressHash) + queryItems.join('&'));
   QNetworkRequest request(url);

   auto reply = _namStorage.localData()->get(request);

   connect(reply, &QNetworkReply::finished, this, [&, reply]() {
      if (reply->error() == QNetworkReply::NoError)
         setPixmap(QPixmap::fromImage(QImage::fromData(reply->readAll())));
   });
}
