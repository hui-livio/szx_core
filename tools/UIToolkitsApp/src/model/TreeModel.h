/**
 * @file TreeModel.h
 * @brief 树形数据模型，暴露给 QML 使用
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-01
 */

#pragma once

#include <QAbstractItemModel>
#include <QList>
#include <QVariant>
#include "TreeNode.h"

class TreeModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    enum Roles
    {
        TitleRole = Qt::UserRole + 1,
        IconRole,
        ExtraDataRole,
        ChildrenRole,
        HasChildrenRole
    };

    explicit TreeModel(QObject *parent = nullptr);

    // QAbstractItemModel 接口
    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // 数据操作
    void setTreeData(const QList<TreeNode> &items);
    const QList<TreeNode> &treeData() const;

private:
    const TreeNode *nodeFromIndex(const QModelIndex &index) const;
    QList<TreeNode> m_treeData;
};
