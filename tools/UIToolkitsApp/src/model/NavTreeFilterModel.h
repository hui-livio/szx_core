/**
 * @file NavTreeFilterModel.h
 * @brief 导航树递归过滤代理模型
 * @author livliu(yuanfeng1897@outlook.com)
 * @date   2025-06-17
 */

#pragma once

#include <QSortFilterProxyModel>

class NavTreeFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)

public:
    explicit NavTreeFilterModel(QObject *parent = nullptr);

    QString filterText() const;
    void setFilterText(const QString &text);

signals:
    void filterTextChanged(const QString &text);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_filterText;
    int m_titleRole;
};
