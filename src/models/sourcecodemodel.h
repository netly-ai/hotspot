
/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "data.h"
#include "disassemblyoutput.h"

#include <memory>
#include <QAbstractTableModel>
#include <QTextLine>

class QTextDocument;

namespace KSyntaxHighlighting {
class SyntaxHighlighter;
class Repository;
}

Q_DECLARE_METATYPE(QTextLine);

class SourceCodeModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit SourceCodeModel(QObject* parent = nullptr);
    ~SourceCodeModel();

    void setDisassembly(const DisassemblyOutput& disassemblyOutput);

    void clear();

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    int lineForIndex(const QModelIndex& index);

    enum Columns
    {
        SourceCodeColumn,
        SourceCodeLineNumber,
        Highlight,
        COLUMN_COUNT
    };

    enum CustomRoles
    {
        CostRole = Qt::UserRole,
        TotalCostRole
    };

public slots:
    void updateHighlighting(int line);
    void setCallerCalleeResults(const Data::CallerCalleeResults& results);
    void updateColorTheme();

private:
    QTextDocument* m_document;
    std::unique_ptr<KSyntaxHighlighting::Repository> m_repository;
    KSyntaxHighlighting::SyntaxHighlighter* m_highlighter = nullptr;
    QSet<int> m_validLineNumbers;
    QList<QTextLine> m_sourceCode;
    Data::CallerCalleeResults m_callerCalleeResults;
    Data::Costs m_costs;
    int m_numTypes = 0;
    int m_lineOffset = 0;
    int m_highlightLine = 0;
};
