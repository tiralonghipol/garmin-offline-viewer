#include "ActivityListPage.hpp"

#include <QButtonGroup>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QStackedLayout>
#include <QVBoxLayout>

#include "ActivityCardDelegate.hpp"

ActivityListPage::ActivityListPage(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("page"));
    setAttribute(Qt::WA_StyledBackground);

    auto* title = new QLabel(tr("Activities"));
    title->setObjectName(QStringLiteral("pageTitle"));
    source_ = new QLabel;
    source_->setProperty("role", "muted");

    auto* search = new QLineEdit;
    search->setObjectName(QStringLiteral("search"));
    search->setPlaceholderText(tr("Search activities"));
    search->setClearButtonEnabled(true);

    // Segmented sport filter
    auto* filters = new QHBoxLayout;
    filters->setSpacing(0);
    auto* group = new QButtonGroup(this);
    // Button id = category + 1, 0 = all. (Not -1: QButtonGroup treats -1 as "assign an id".)
    constexpr int kAll = 0;
    const auto idOf = [](theme::Category c) { return static_cast<int>(c) + 1; };
    const std::vector<std::pair<QString, int>> buttons{
        {tr("All"), kAll},
        {QStringLiteral("🏃 ") + tr("Running"), idOf(theme::Category::Running)},
        {QStringLiteral("🚴 ") + tr("Cycling"), idOf(theme::Category::Cycling)},
        {QStringLiteral("💪 ") + tr("Other"), idOf(theme::Category::Other)},
    };
    for (const auto& [text, id] : buttons) {
        auto* b = new QPushButton(text);
        b->setProperty("role", "filter");
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        group->addButton(b, id);
        filters->addWidget(b);
    }
    group->button(kAll)->setChecked(true);

    auto* toolbar = new QHBoxLayout;
    toolbar->addWidget(search);
    toolbar->addSpacing(12);
    toolbar->addLayout(filters);
    toolbar->addStretch();

    model_ = new ActivityListModel(this);
    filter_ = new ActivityFilterModel(this);
    filter_->setSourceModel(model_);

    list_ = new QListView;
    list_->setObjectName(QStringLiteral("activityList"));
    list_->setModel(filter_);
    list_->setItemDelegate(new ActivityCardDelegate(list_));
    list_->setMouseTracking(true);  // hover highlight
    list_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list_->setUniformItemSizes(true);
    list_->setCursor(Qt::PointingHandCursor);

    // Empty / loading state
    auto* empty = new QWidget;
    auto* emptyLayout = new QVBoxLayout(empty);
    emptyText_ = new QLabel;
    emptyText_->setProperty("role", "muted");
    emptyText_->setAlignment(Qt::AlignCenter);
    auto* openButton = new QPushButton(tr("Open watch folder…"));
    openButton->setProperty("role", "primary");
    emptyLayout->addStretch();
    emptyLayout->addWidget(emptyText_);
    emptyLayout->addWidget(openButton, 0, Qt::AlignHCenter);
    emptyLayout->addStretch(2);

    auto* stackHost = new QWidget;
    stack_ = new QStackedLayout(stackHost);
    stack_->addWidget(empty);
    stack_->addWidget(list_);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 22, 28, 12);
    layout->addWidget(title);
    layout->addWidget(source_);
    layout->addSpacing(12);
    layout->addLayout(toolbar);
    layout->addSpacing(12);
    layout->addWidget(stackHost, 1);

    connect(search, &QLineEdit::textChanged, filter_, &ActivityFilterModel::setSearchText);
    connect(group, &QButtonGroup::idClicked, this, [this](int id) {
        filter_->setCategory(id == kAll ? std::nullopt : std::optional{static_cast<theme::Category>(id - 1)});
    });
    connect(list_, &QListView::clicked, this, [this](const QModelIndex& index) {
        emit activityActivated(index.data(ActivityListModel::EntryRole).value<ActivityEntry>().path);
    });
    connect(list_, &QListView::activated, this, [this](const QModelIndex& index) {  // Enter key
        emit activityActivated(index.data(ActivityListModel::EntryRole).value<ActivityEntry>().path);
    });
    connect(openButton, &QPushButton::clicked, this, &ActivityListPage::openFolderRequested);

    setBusy(tr("No activities loaded. Plug in your watch or open a folder with .fit files."));
}

void ActivityListPage::setEntries(std::vector<ActivityEntry> entries, const QString& source) {
    const auto count = static_cast<int>(entries.size());
    model_->setEntries(std::move(entries));
    source_->setText(tr("%n activities · %1", nullptr, count).arg(QDir::toNativeSeparators(source)));
    if (count == 0) {
        emptyText_->setText(tr("No .fit files found in %1").arg(QDir::toNativeSeparators(source)));
    }
    stack_->setCurrentIndex(count > 0 ? 1 : 0);
}

void ActivityListPage::setBusy(const QString& message) {
    emptyText_->setText(message);
    stack_->setCurrentIndex(0);
}
