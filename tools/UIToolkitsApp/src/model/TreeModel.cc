/**
 * @file TreeModel.cc
 * @brief 树形数据模型实现
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-01
 */

#include "TreeModel.h"

TreeModel::TreeModel(QObject *parent) :
    QAbstractItemModel(parent)
{
}

QModelIndex TreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return {};

    if (!parent.isValid())
    {
        // 根层级
        if (row >= m_treeData.size())
            return {};
        return createIndex(row, column, const_cast<TreeNode *>(&m_treeData.at(row)));
    }

    const TreeNode *parentNode = static_cast<const TreeNode *>(parent.internalPointer());
    if (row >= parentNode->children.size())
        return {};

    return createIndex(row, column, const_cast<TreeNode *>(&parentNode->children.at(row)));
}

QModelIndex TreeModel::parent(const QModelIndex &child) const
{
    if (!child.isValid())
        return {};

    const TreeNode *childNode = static_cast<const TreeNode *>(child.internalPointer());

    // 查找父节点
    for (int i = 0; i < m_treeData.size(); ++i)
    {
        const TreeNode &node = m_treeData.at(i);
        for (const TreeNode &c : node.children)
        {
            if (&c == childNode)
                return createIndex(i, 0, const_cast<TreeNode *>(&node));
        }
    }

    return {};
}

int TreeModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;

    const TreeNode *parentNode = nodeFromIndex(parent);
    if (!parentNode)
        return m_treeData.size();

    return parentNode->children.size();
}

int TreeModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 1;
}

QVariant TreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return {};

    const TreeNode *node = static_cast<const TreeNode *>(index.internalPointer());

    switch (role)
    {
    case TitleRole:
        return node->title;
    case IconRole:
        return node->icon;
    case ExtraDataRole:
        return node->extraData;
    case ChildrenRole:
    {
        QVariantList childList;
        for (const TreeNode &child : node->children)
        {
            QVariantMap childMap;
            childMap["title"]     = child.title;
            childMap["icon"]      = child.icon;
            childMap["extraData"] = child.extraData;
            childList.append(childMap);
        }
        return childList;
    }
    case HasChildrenRole:
        return !node->children.isEmpty();
    default:
        return {};
    }
}

QHash<int, QByteArray> TreeModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[TitleRole]       = "title";
    roles[IconRole]        = "icon";
    roles[ExtraDataRole]   = "extraData";
    roles[ChildrenRole]    = "children";
    roles[HasChildrenRole] = "hasChildren";
    return roles;
}

void TreeModel::setTreeData(const QList<TreeNode> &items)
{
    beginResetModel();
    m_treeData = items;
    endResetModel();
}

const QList<TreeNode> &TreeModel::treeData() const
{
    return m_treeData;
}

const TreeNode *TreeModel::nodeFromIndex(const QModelIndex &index) const
{
    if (!index.isValid())
        return nullptr;
    return static_cast<const TreeNode *>(index.internalPointer());
}
