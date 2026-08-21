/**
 * @file NavTreeFilterModel.cc
 * @brief 导航树递归过滤代理模型实现
 * @author livliu(yuanfeng1897@outlook.com)
 * @date   2025-06-17
 */

#include "NavTreeFilterModel.h"
#include "TreeModel.h"

NavTreeFilterModel::NavTreeFilterModel(QObject *parent) :
    QSortFilterProxyModel(parent),
    m_titleRole(TreeModel::TitleRole)
{
    setRecursiveFilteringEnabled(true);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
}

QString NavTreeFilterModel::filterText() const
{
    return m_filterText;
}

void NavTreeFilterModel::setFilterText(const QString &text)
{
    qDebug() << "setFilterText" << text;
    if (m_filterText == text)
        return;
    m_filterText = text;
    beginFilterChange();
    endFilterChange();

    // qDebug() << "after filter change:" << rowCount();

    emit filterTextChanged(m_filterText);
}

bool NavTreeFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (m_filterText.isEmpty())
        return true;
    // qDebug() << "filterAcceptsRow" << sourceRow << m_filterText;


    QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
    QString title   = sourceModel()->data(idx, m_titleRole).toString();

    // recursiveFilteringEnabled(true) 已处理子节点递归：
    // 若任一子孙匹配，Qt 会自动保留该父行。此处只需判断本行自身。
    return title.contains(m_filterText, Qt::CaseInsensitive);
}
