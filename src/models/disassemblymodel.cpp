/*
    SPDX-FileCopyrightText: Darya Knysh <d.knysh@nips.ru>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QPalette>
#include <QTextBlock>
#include <QTextDocument>

#include "disassemblymodel.h"
#include "hotspot-config.h"

#include "sourcecodemodel.h"

#ifdef KF5SyntaxHighlighting_FOUND
#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Repository>
#include <KSyntaxHighlighting/SyntaxHighlighter>
#include <KSyntaxHighlighting/Theme>
#endif

DisassemblyModel::DisassemblyModel(QObject* parent)
    : QAbstractTableModel(parent)
    , m_document(new QTextDocument(this))
#ifdef KF5SyntaxHighlighting_FOUND
    , m_repository(std::make_unique<KSyntaxHighlighting::Repository>())
    , m_highlighter(new KSyntaxHighlighting::SyntaxHighlighter(m_document))
#endif
{
    updateColorTheme();
}

DisassemblyModel::~DisassemblyModel() = default;

void DisassemblyModel::clear()
{
    beginResetModel();
    m_data = {};
    endResetModel();
}

QModelIndex DisassemblyModel::findIndexWithOffset(int offset)
{
    quint64 address = m_data.disassemblyLines[0].addr + offset;

    const auto& found =
        std::find_if(m_data.disassemblyLines.begin(), m_data.disassemblyLines.end(),
                     [address](const DisassemblyOutput::DisassemblyLine& line) { return line.addr == address; });

    if (found != m_data.disassemblyLines.end()) {
        return createIndex(std::distance(m_data.disassemblyLines.begin(), found), DisassemblyColumn);
    }
    return {};
}

void DisassemblyModel::setDisassembly(const DisassemblyOutput& disassemblyOutput)
{
    beginResetModel();
    m_data = disassemblyOutput;
    m_lines.clear();

    QString sourceCode;

    for (const auto& it : disassemblyOutput.disassemblyLines) {
        sourceCode += it.disassembly + QLatin1Char('\n');
    }

    m_document->setPlainText(sourceCode);
    m_document->setTextWidth(m_document->idealWidth());

#ifdef KF5SyntaxHighlighting_FOUND
    const auto def = m_repository->definitionForName(QStringLiteral("GNU Assembler"));
    m_highlighter->setDefinition(def);
#endif

    for (auto block = m_document->firstBlock(); block != m_document->lastBlock(); block = block.next()) {
        m_lines.push_back(block.layout()->lineAt(0));
    }

    endResetModel();
}

void DisassemblyModel::setResults(const Data::CallerCalleeResults& results)
{
    beginResetModel();
    m_results = results;
    m_numTypes = results.selfCosts.numTypes();
    endResetModel();
}

QVariant DisassemblyModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (section < 0 || section >= m_numTypes + COLUMN_COUNT)
        return {};
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return {};

    if (section == DisassemblyColumn)
        return tr("Assembly");

    if (section == LinkedFunctionName) {
        return tr("Linked Function Name");
    }

    if (section == LinkedFunctionOffset) {
        return tr("Linked Function Offset");
    }

    if (section == SourceCodeLine) {
        return tr("Source Code Line");
    }

    if (section == Highlight) {
        return tr("Highlight");
    }

    if (section - COLUMN_COUNT <= m_numTypes)
        return m_results.selfCosts.typeName(section - COLUMN_COUNT);

    return {};
}

QVariant DisassemblyModel::data(const QModelIndex& index, int role) const
{
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return {};

    if (index.row() > m_data.disassemblyLines.count() || index.row() < 0)
        return {};

    const auto &data = m_data.disassemblyLines.at(index.row());

    if (role == Qt::DisplayRole || role == CostRole || role == TotalCostRole || role == Qt::ToolTipRole) {
        if (index.column() == DisassemblyColumn)
            return QVariant::fromValue(m_lines[index.row()]);
        // return data.disassembly;

        if (index.column() == LinkedFunctionName)
            return data.linkedFunction.name;

        if (index.column() == LinkedFunctionOffset)
            return data.linkedFunction.offset;

        if (index.column() == SourceCodeLine)
            return data.sourceCodeLine;

        if (index.column() == Highlight)
            return data.sourceCodeLine == m_highlightLine;

        if (data.addr == 0) {
            return {};
        }

        auto results = m_results;
        auto entry = results.entry(m_data.symbol);
        auto it = entry.offsetMap.find(data.addr);
        if (it != entry.offsetMap.end()) {
            int event = index.column() - COLUMN_COUNT;

            const auto &locationCost = it.value();
            const auto &costLine = locationCost.selfCost[event];
            const auto totalCost = m_results.selfCosts.totalCost(event);

            if (role == CostRole)
                return costLine;
            if (role == TotalCostRole)
                return totalCost;
            if (role == Qt::ToolTipRole)
                return Util::formatTooltip(data.disassembly, locationCost, m_results.selfCosts);

            return Util::formatCostRelative(costLine, totalCost, true);
        } else {
            if (role == Qt::ToolTipRole)
                return tr("<qt><tt>%1</tt><hr/>No samples at this location.</qt>").arg(data.disassembly.toHtmlEscaped());
            else
                return QString();
        }
    }
    return {};
}

int DisassemblyModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : COLUMN_COUNT + m_numTypes;
}

int DisassemblyModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_data.disassemblyLines.count();
}

void DisassemblyModel::updateHighlighting(int line)
{
    m_highlightLine = line;
    emit dataChanged(createIndex(0, Columns::DisassemblyColumn), createIndex(rowCount(), Columns::DisassemblyColumn));
}

int DisassemblyModel::lineForIndex(const QModelIndex& index)
{
    return m_data.disassemblyLines[index.row()].sourceCodeLine;
}

void DisassemblyModel::updateColorTheme()
{
#ifdef KF5SyntaxHighlighting_FOUND
    KSyntaxHighlighting::Repository::DefaultTheme theme;
    if (QPalette().base().color().lightness() < 128) {
        theme = KSyntaxHighlighting::Repository::DarkTheme;
    } else {
        theme = KSyntaxHighlighting::Repository::LightTheme;
    }
    m_highlighter->setTheme(m_repository->defaultTheme(theme));
    m_highlighter->rehighlight();
#endif
}
