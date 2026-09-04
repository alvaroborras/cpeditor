/*
 * Copyright (C) 2019-2021 Ashar Khan <ashar786khan@gmail.com>
 *
 * This file is part of CP Editor.
 *
 * CP Editor is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * I will not be responsible if CP Editor behaves in unexpected way and
 * causes your ratings to go down and or lose any important contest.
 *
 * Believe Software is "Software" and it isn't immune to bugs.
 *
 */
#include "LanguageServer.hpp"
#include <algorithm>
#include "Extensions/LSPCompleter.hpp"
#include "Core/EventLogger.hpp"
#include "Core/MessageLogger.hpp"
#include "Settings/SettingsManager.hpp"
#include "Util/Util.hpp"
#include "third_party/lsp-cpp/include/LSPClient.hpp"
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

namespace Extensions
{

LanguageServer::LanguageServer(QString const &lang)
{
    LOG_INFO(INFO_OF(lang));
    this->language = lang;
    if (shouldCreateClient())
    {
        createClient();
        performConnection();
    }
}

LanguageServer::~LanguageServer()
{
    if (lsp != nullptr)
    {
        LOG_INFO("Killing LSP");
        lsp->shutdown();
        lsp->exit();
        delete lsp;
    }
}

void LanguageServer::openDocument(QString const &path, Editor::CodeEditor *editor, MessageLogger *log)
{
    if (isDocumentOpen())
    {
        LOG_WARN("openDocument called without closing the previous document. Closing it now");
        closeDocument();
    }

    m_editor = editor;
    openFile = path;
    logger = log;

    if (lsp == nullptr)
        return;

    if (!isInitialized)
    {
        initializeLSP(path);
        isInitialized = true;
    }

    std::string uri = "file://" + path.toStdString();
    std::string code = m_editor->toPlainText().toStdString();
    std::string lang;

    if (language == "Java")
        lang = "java";
    else if (language == "Python")
        lang = "python";
    else
    {
        LOG_WARN_IF(language != "C++", "Unknown language " << language);
        lang = "cpp";
    }

    lsp->didOpen(uri, code, lang);
    synchronizedText = QString::fromStdString(code);
}

void LanguageServer::closeDocument()
{
    LOG_WARN_IF(!isDocumentOpen(), "Cannot close the document, No document was open");
    if (!isDocumentOpen())
        return;

    std::string uri = "file://" + openFile.toStdString();
    lsp->didClose(uri);

    openFile = "";
    logger = nullptr;
    m_editor = nullptr;
    completer = nullptr;
    synchronizedText.clear();
    completionRequestInFlight = false;
    completionRequestText.clear();
    completionRequestLine = -1;
    completionRequestCharacter = -1;
}

void LanguageServer::requestLinting()
{
    if (m_editor == nullptr || !isDocumentOpen())
        return;
    const auto currentText = m_editor->toPlainText();
    if (currentText == synchronizedText)
        return;

    std::vector<TextDocumentContentChangeEvent> changes;
    TextDocumentContentChangeEvent e;
    e.text = currentText.toStdString();
    changes.push_back(e);

    const std::string uri = "file://" + openFile.toStdString();
    lsp->didChange(uri, changes, true);
    synchronizedText = currentText;
}
void LanguageServer::requestCompletion(int lineNumber, int characterNumber, LSPCompleter *completionTarget)
{
    if (m_editor == nullptr || !isDocumentOpen() || completionTarget == nullptr)
        return;
    if (completionRequestInFlight)
        return;

    const std::string uri = "file://" + openFile.toStdString();
    const auto currentText = m_editor->toPlainText();
    if (currentText != synchronizedText)
    {
        std::vector<TextDocumentContentChangeEvent> changes;
        TextDocumentContentChangeEvent change;
        change.text = currentText.toStdString();
        changes.push_back(change);
        lsp->didChange(uri, changes, false);
        synchronizedText = currentText;
    }
    completer = completionTarget;
    Position position;
    position.line = lineNumber;
    position.character = characterNumber;
    CompletionContext context;
    context.triggerKind = CompletionTriggerKind::Invoked;
    completionRequestInFlight = true;
    completionRequestText = currentText;
    completionRequestLine = lineNumber;
    completionRequestCharacter = characterNumber;
    lsp->completion(uri, position, context);
}

bool LanguageServer::isDocumentOpen() const
{
    return !openFile.isEmpty() && lsp != nullptr;
}

void LanguageServer::updateSettings()
{
    if (lsp != nullptr)
    {
        LOG_INFO("Killing LSP");
        lsp->shutdown();
        lsp->exit();
        delete lsp;
        lsp = nullptr;
    }

    if (m_editor != nullptr)
        m_editor->clearSquiggle();

    if (shouldCreateClient())
    {
        createClient();

        performConnection();
        initializeLSP(openFile);

        LOG_INFO("Recreated Language server Process");
        if (m_editor != nullptr)
        {
            auto *tmpEditor = m_editor;
            auto tmpPath = openFile;
            auto *tmpLog = logger;
            if (isDocumentOpen())
                closeDocument();
            openDocument(tmpPath, tmpEditor, tmpLog);
            LOG_INFO("Reopened document after restart");
        }
    }
}

void LanguageServer::updatePath(QString const &newPath)
{
    if (lsp == nullptr || (openFile == newPath))
        return;
    auto *tmpLogger = logger;
    auto *tmpEditor = m_editor;
    closeDocument();
    openDocument(newPath, tmpEditor, tmpLogger);
}

// Private methods
bool LanguageServer::shouldCreateClient()
{
    return SettingsManager::get("LSP/Use Linting " + language).toBool() ||
           SettingsManager::get("LSP/Use Autocomplete " + language).toBool();
}

void LanguageServer::createClient()
{
    delete lsp;
    auto program = SettingsManager::get("LSP/Path " + language).toString();
    auto args = QProcess::splitCommand(SettingsManager::get("LSP/Args " + language).toString().trimmed());
    lsp = new LSPClient(program, args);
}

void LanguageServer::performConnection()
{
    if (lsp == nullptr)
    {
        LOG_WARN("Skipping establishement of connections as lsp client is nullptr");
        return;
    }
    connect(lsp, &LSPClient::onError, this, &LanguageServer::onLSPServerErrorArrived);
    connect(lsp, &LSPClient::onRequest, this, &LanguageServer::onLSPServerRequestArrived);
    connect(lsp, &LSPClient::onServerError, this, &LanguageServer::onLSPServerProcessError);
    connect(lsp, &LSPClient::onResponse, this, &LanguageServer::onLSPServerResponseArrived);
    connect(lsp, &LSPClient::onNotify, this, &LanguageServer::onLSPServerNotificationArrived);
    connect(lsp, &LSPClient::onServerFinished, this, &LanguageServer::onLSPServerProcessFinished);
    connect(lsp, &LSPClient::newStderr, this, &LanguageServer::onLSPServerNewStderr);

    LOG_INFO("All language server connections have been established");
}

Editor::CodeEditor::SeverityLevel LanguageServer::lspSeverity(int in)
{
    switch (in)
    {
    case 1:
        return Editor::CodeEditor ::SeverityLevel::Error;
    case 2:
        return Editor::CodeEditor::SeverityLevel::Warning;
    case 3:
        return Editor::CodeEditor::SeverityLevel::Information;
    case 4:
        return Editor::CodeEditor::SeverityLevel::Hint;
    default:
        return Editor::CodeEditor::SeverityLevel::Error;
    }
    // Nothing matched
    return Editor::CodeEditor::SeverityLevel::Error;
}

void LanguageServer::initializeLSP(QString const &filePath)
{
    QFileInfo info(filePath);
    std::string uri = "file://" + info.absoluteDir().absolutePath().toStdString();
    option<DocumentUri> rootUri(uri);
    lsp->initialize(rootUri);
}
// ---------------------------- LSP SLOTS ------------------------

void LanguageServer::onLSPServerNotificationArrived(QString const &method, QJsonObject const &param)
{
    if (method == "textDocument/publishDiagnostics" && m_editor != nullptr) // Linting
    {
        m_editor->clearSquiggle();
        QJsonArray doc = QJsonDocument::fromVariant(param.toVariantMap()).object()["diagnostics"].toArray();
        for (auto e : doc)
        {
            QString tooltip = e.toObject()["message"].toString();
            Editor::CodeEditor::SeverityLevel level = lspSeverity(e.toObject()["severity"].toInt());

            auto beg = e.toObject()["range"].toObject()["start"].toObject();
            auto end = e.toObject()["range"].toObject()["end"].toObject();

            QPair<int, int> start;
            QPair<int, int> stop;

            start.first = beg["line"].toInt() + 1;
            start.second = beg["character"].toInt();

            stop.first = end["line"].toInt() + 1;
            stop.second = end["character"].toInt();

            m_editor->addSquiggle(
                level, start, stop,
                tooltip.remove(" (fix available)")); // We do not provide quick fix so remove this text.
        }
        m_editor->highlightAllSquiggle();
    }
}

void LanguageServer::onLSPServerResponseArrived(QJsonObject const &id, QJsonObject const &response)
{
    Q_UNUSED(id);
    // The initialize response is also delivered through this signal.
    if (response.contains("capabilities"))
    {
        lsp->initialized();
        return;
    }
    if (!completionRequestInFlight || completer == nullptr || m_editor == nullptr)
        return;
    completionRequestInFlight = false;
    if (m_editor->toPlainText() != completionRequestText || m_editor->textCursor().blockNumber() != completionRequestLine ||
        m_editor->textCursor().positionInBlock() != completionRequestCharacter)
    {
        completer->clearCompletion();
        return;
    }

    const auto items = response.value("items").toArray();

    auto stripSnippet = [](QString text) {
        QRegularExpression placeholder(R"(\$\{\d+:([^{}]*)\})");
        QRegularExpression choice(R"(\$\{\d+\|([^}]*)\})");
        QRegularExpression tabstop(R"(\$\d+)");
        QRegularExpressionMatch match;
        while ((match = placeholder.match(text)).hasMatch())
            text.replace(match.capturedStart(), match.capturedLength(), match.captured(1));
        while ((match = choice.match(text)).hasMatch())
            text.replace(match.capturedStart(), match.capturedLength(), match.captured(1).section(',', 0, 0));
        text.remove(tabstop);
        text.replace("\\\\$", "$");
        return text;
    };

    auto readPosition = [](const QJsonObject &object, int &line, int &character) {
        if (!object.contains("line") || !object.contains("character"))
            return false;
        line = object.value("line").toInt(-1);
        character = object.value("character").toInt(-1);
        return line >= 0 && character >= 0;
    };

    QVector<CompletionItem> completions;
    for (const auto &value : items)
    {
        const auto item = value.toObject();
        CompletionItem completion;
        completion.label = item.value("label").toString();
        completion.insertText = item.value("insertText").toString();
        completion.filterText = item.value("filterText").toString();
        completion.sortText = item.value("sortText").toString();
        completion.detail = item.value("detail").toString();

        const auto documentation = item.value("documentation");
        if (documentation.isString())
            completion.documentation = documentation.toString();
        else if (documentation.isObject())
            completion.documentation = documentation.toObject().value("value").toString();

        const auto textEdit = item.value("textEdit").toObject();
        const auto newText = textEdit.value("newText").toString();
        if (!newText.isEmpty())
            completion.insertText = newText;
        if (!completion.insertText.isEmpty() && completion.label.isEmpty())
            completion.label = completion.insertText;
        if (completion.insertText.isEmpty())
            completion.insertText = completion.label;
        if (completion.label.isEmpty())
            continue;

        const auto range = textEdit.value("range").toObject();
        completion.hasTextEdit = readPosition(range.value("start").toObject(), completion.startLine,
                                               completion.startCharacter) &&
                                 readPosition(range.value("end").toObject(), completion.endLine,
                                               completion.endCharacter);
        completion.isSnippet = item.value("insertTextFormat").toInt() == 2;
        if (completion.isSnippet)
            completion.insertText = stripSnippet(completion.insertText);
        completions.append(completion);
    }
    if (std::any_of(completions.cbegin(), completions.cend(), [](const CompletionItem &completion) {
            return !completion.sortText.isEmpty();
        }))
    {
        std::stable_sort(completions.begin(), completions.end(), [](const CompletionItem &left, const CompletionItem &right) {
            const auto leftKey = left.sortText.isEmpty() ? left.label : left.sortText;
            const auto rightKey = right.sortText.isEmpty() ? right.label : right.sortText;
            return leftKey < rightKey;
        });
    }
    completer->setCompletions(completions);
}

void LanguageServer::onLSPServerRequestArrived(QString const &method, // NOLINT: It can be made static.
                                               QJsonObject const &param, QJsonObject const &id)
{
    LOG_INFO("Request from Sever has arrived. " << INFO_OF(method));
}

void LanguageServer::onLSPServerErrorArrived(QJsonObject const &id, QJsonObject const &error)
{
    completionRequestInFlight = false;
    QString ID;
    QString ERR;
    ID = QJsonDocument::fromVariant(id.toVariantMap()).toJson();
    ERR = QJsonDocument::fromVariant(error.toVariantMap()).toJson();

    LOG_ERR("ID is \n" << ID);
    LOG_ERR("ERR is \n" << ERR);

    if (logger != nullptr)
        logger->error(tr("Language Server [%1]").arg(language),
                      tr("Language server sent an error. Please check log for details."));
}

void LanguageServer::onLSPServerProcessError(QProcess::ProcessError const &error)
{
    LOG_WARN_IF(error == QProcess::Crashed, "LSP Process errored out " << INFO_OF(error));
    LOG_ERR_IF(error != QProcess::Crashed, "LSP Process errored out " << INFO_OF(error));
    if (logger == nullptr)
        return;
    switch (error)
    {
    case QProcess::FailedToStart:
        logger->error(tr("Language Server [%1]").arg(language),
                      tr("Failed to start LSP Process. Have you set the path to the Language Server program at %1?")
                          .arg(SettingsManager::getPathText("LSP/Path " + language)),
                      false);
        break;
    case QProcess::Crashed:
        break;
    case QProcess::Timedout:
        logger->error(tr("Language Server [%1]").arg(language), tr("LSP Process timed out"));
        break;
    case QProcess::ReadError:
        logger->error(tr("Language Server [%1]").arg(language), tr("LSP Process Read Error"));
        break;
    case QProcess::WriteError:
        logger->error(tr("Language Server [%1]").arg(language), tr("LSP Process Write Error"));
        break;
    case QProcess::UnknownError:
        logger->error(tr("Language Server [%1]").arg(language), tr("An unknown error has occurred in LSP Process"));
        break;
    }
}

void LanguageServer::onLSPServerProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    LOG_INFO_IF(exitCode == 0, "LSP Finished with exit code " << exitCode << INFO_OF(language) << INFO_OF(status));
    LOG_WARN_IF(exitCode != 0, "LSP Finished with exit code " << exitCode << INFO_OF(language) << INFO_OF(status));
}

void LanguageServer::onLSPServerNewStderr(const QString &content) // NOLINT: It can be made static
{
    LOG_INFO(content);
}
} // namespace Extensions
