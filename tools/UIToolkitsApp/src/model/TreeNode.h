/**
 * @file TreeNode.h
 * @brief 树形节点数据结构
 * @author livliu(yuanfeng1897@outlook.com)
 * @date 2026-07-01
 */

#pragma once

#include <QList>
#include <QString>
#include <QVariantMap>

struct TreeNode
{
    QString title;
    QString icon;
    QVariantMap extraData;
    QList<TreeNode> children;

    bool operator==(const TreeNode &other) const
    {
        return title == other.title && icon == other.icon;
    }
};
