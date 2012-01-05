#ifndef DATAWIDGET_H
#define DATAWIDGET_H

#include <QFrame>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>

#include "../packet.h"

enum WidgetTypes
{
    WIDGET_NUMBERS,
    WIDGET_BAR
    //TODO: graf, barva, X Y mapa, rafickovej ukazatel, timestamp
};

enum NumberTypes
{
    NUM_UINT8,
    NUM_UINT16,
    NUM_UINT32,
    NUM_UINT64,

    NUM_INT8,
    NUM_INT16,
    NUM_INT32,
    NUM_INT64,

    NUM_FLOAT,
    NUM_DOUBLE,

    NUM_COUNT
};

enum DragActions
{
    DRAG_MOVE       = 0x01,
    DRAG_RES_LEFT   = 0x02,
    DRAG_RES_RIGHT  = 0x04,
    //DRAG_RES_TOP    = 0x08, // Unused
    DRAG_RES_BOTTOM = 0x10
};

#define RESIZE_BORDER 15 // number of pixels from every side which counts as resize drag

struct data_widget_info
{
    qint16 device;
    qint16 command;
    quint32 pos;
};

class CloseLabel;

class DataWidget : public QFrame
{
    Q_OBJECT

Q_SIGNALS:
    void updateData();

public:
    explicit DataWidget(QWidget *parent = 0);
    ~DataWidget();

    virtual void setUp();

    void setId(quint32 id) { m_id = id; }
    quint32 getId() { return m_id; }

    void setInfo(qint16 device, qint16 command, quint32 pos)
    {
        m_info.device = device;
        m_info.command = command;
        m_info.pos = pos;
    }
    data_widget_info getInfo() { return m_info; }

public slots:
    void newData(analyzer_data *data);

protected:
    void mousePressEvent(QMouseEvent * event);
    void mouseMoveEvent(QMouseEvent * event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);
    void contextMenuEvent ( QContextMenuEvent * event );

    virtual void processData(analyzer_data *data);

    inline void setTitle(QString title);

    quint8 m_widgetType;
    data_widget_info m_info;
    bool m_assigned;

    QVBoxLayout *layout;
    QMenu *contextMenu;

private slots:
    void lockTriggered();
    void setTitleTriggered();

private:
    inline bool iw(int w) { return w + width() > ((QWidget*)parent())->width(); }
    inline bool ih(int h) { return h + height() > ((QWidget*)parent())->height(); }
    quint8 getDragAction(const QPoint& clickPos);
    void dragResize(QMouseEvent* e);
    void dragMove(QMouseEvent* e);

    QPoint mOrigin;
    quint8 m_dragAction;
    bool m_locked;

    QAction *m_lockAction;

    CloseLabel *m_closeLabel;
    quint32 m_id;
};

class DataWidgetAddBtn : public QPushButton
{
    Q_OBJECT
public:
    explicit DataWidgetAddBtn(QWidget *parent);
    ~DataWidgetAddBtn();

protected:
    void mousePressEvent(QMouseEvent *event);

    virtual QPixmap getRender();

    quint8 m_widgetType;
};

class CloseLabel : public QLabel
{
    Q_OBJECT
public:
    explicit CloseLabel(QWidget *parent);

    void setLocked(bool locked);

protected:
    void mousePressEvent(QMouseEvent *event);

private:
    bool m_locked;

};

#endif // DATAWIDGET_H
