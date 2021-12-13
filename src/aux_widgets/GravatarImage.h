#ifndef GRAVATARIMAGE_H
#define GRAVATARIMAGE_H

#include <QLabel>
#include <QNetworkAccessManager>
#include <QThreadStorage>

class GravatarImage
:	public QLabel
{
Q_OBJECT
Q_PROPERTY(QString emailAddress READ emailAddress WRITE setEmailAddress NOTIFY emailAddressChanged)

signals:
   void emailAddressChanged();

public:
   GravatarImage(QWidget* parent = nullptr);

   void setEmailAddress(const QString& emailAddress);
   QString emailAddress() const;

   enum class DefaultImage
   {
      None,
      MysteryPerson,
      Identicon,
      MonsterId,
      Wavatar,
      Retro,
      RoboHash,
      Blank
   };
   void setDefaultImage(DefaultImage defaultImage);
   DefaultImage defaultImage() const;

   enum class Rating
   {
      None,
      G,
      PG,
      R,
      X
   };
   void setRating(Rating rating);
   Rating rating() const;

   bool hasHeightForWidth() const override;
   int heightForWidth(int width) const override;
   QSize sizeHint() const override;

protected:
   void resizeEvent(QResizeEvent* event) override;
   void timerEvent(QTimerEvent* event) override;

private:
   void delayedReload();
   void reload();

   QString _emailAddress;
   QString _emailAddressHash;
   DefaultImage _defaultImage = DefaultImage::None;
   Rating _rating = Rating::None;

   QThreadStorage<QNetworkAccessManager*> _namStorage;
   int _reloadTimer = 0;
};

#endif // GRAVATARIMAGE_H
