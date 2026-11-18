#include "languageworkflow.h"

#include <QCoreApplication>
#include <QLocale>
#include <QTranslator>

#include <QtDebug>

LanguageWorkflow::LanguageWorkflow() = default;

LanguageWorkflow::~LanguageWorkflow()
{
    clearTranslator();
}

LanguageWorkflow::Decision LanguageWorkflow::planChange(
    int currentLanguage,
    int requestedLanguage,
    bool dynamicRetranslationSupported)
{
    if (!isValidLanguage(requestedLanguage)) {
        return { Status::InvalidInput, currentLanguage, false, false, false };
    }
    if (requestedLanguage == currentLanguage) {
        return { Status::Unchanged, currentLanguage, false, false, false };
    }
    if (!dynamicRetranslationSupported) {
        return { Status::ApplyAfterRestart, requestedLanguage, false, false, true };
    }

    // clearOnBack preserves the existing AppView recovery workaround. It must
    // remain part of the successful dynamic-change command until a focused
    // regression test proves that rebuilding the stack is no longer required.
    return { Status::ApplyNow, requestedLanguage, true, true, false };
}

bool LanguageWorkflow::isValidLanguage(int language)
{
    return language >= Automatic && language <= LastLanguage;
}

QString LanguageWorkflow::suffixForLanguage(int language, const QString& systemLocale)
{
    switch (language) {
    case German:
        return QStringLiteral("de");
    case English:
        return QStringLiteral("en");
    case French:
        return QStringLiteral("fr");
    case SimplifiedChinese:
        return QStringLiteral("zh_CN");
    case NorwegianBokmal:
        return QStringLiteral("nb_NO");
    case Russian:
        return QStringLiteral("ru");
    case Spanish:
        return QStringLiteral("es");
    case Japanese:
        return QStringLiteral("ja");
    case Vietnamese:
        return QStringLiteral("vi");
    case Thai:
        return QStringLiteral("th");
    case Korean:
        return QStringLiteral("ko");
    case Hungarian:
        return QStringLiteral("hu");
    case Dutch:
        return QStringLiteral("nl");
    case Swedish:
        return QStringLiteral("sv");
    case Turkish:
        return QStringLiteral("tr");
    case Ukrainian:
        return QStringLiteral("uk");
    case TraditionalChinese:
        return QStringLiteral("zh_TW");
    case Portuguese:
        return QStringLiteral("pt");
    case BrazilianPortuguese:
        return QStringLiteral("pt_BR");
    case Greek:
        return QStringLiteral("el");
    case Italian:
        return QStringLiteral("it");
    case Hindi:
        return QStringLiteral("hi");
    case Polish:
        return QStringLiteral("pl");
    case Czech:
        return QStringLiteral("cs");
    case Hebrew:
        return QStringLiteral("he");
    case CentralKurdish:
        return QStringLiteral("ckb");
    case Lithuanian:
        return QStringLiteral("lt");
    case Estonian:
        return QStringLiteral("et");
    case Bulgarian:
        return QStringLiteral("bg");
    case Esperanto:
        return QStringLiteral("eo");
    case Tamil:
        return QStringLiteral("ta");
    case Automatic:
    default:
        return systemLocale;
    }
}

LanguageWorkflow::Decision LanguageWorkflow::changeLanguage(
    StatePort& state,
    int requestedLanguage,
    bool dynamicRetranslationSupported)
{
    const Decision decision = planChange(
        state.currentLanguage(),
        requestedLanguage,
        dynamicRetranslationSupported);
    if (decision.status == Status::InvalidInput ||
        decision.status == Status::Unchanged) {
        return decision;
    }

    // Preserve the legacy observable order: preference state and notification
    // first, translator install/removal second, renderer refresh in the
    // compatibility adapter last.
    state.commitLanguage(decision.language);
    if (decision.retranslate) {
        applyTranslation(decision.language);
    }
    return decision;
}

bool LanguageWorkflow::applyTranslation(int language)
{
    auto newTranslator = std::make_unique<QTranslator>();
    const QString languageSuffix =
        suffixForLanguage(language, QLocale::system().name());

    // Clear the previous catalog before attempting the new one. A missing
    // catalog (including intentional English source text) must recover to
    // source strings instead of leaving stale translations installed.
    clearTranslator();

    if (newTranslator->load(QStringLiteral(":/languages/qml_") + languageSuffix)) {
        qInfo() << "Successfully loaded translation for" << languageSuffix;
        QCoreApplication::installTranslator(newTranslator.get());
        m_Translator = std::move(newTranslator);
    }
    else {
        qInfo() << "No translation available for" << languageSuffix;
    }

    return true;
}

bool LanguageWorkflow::translationLoaded() const
{
    return m_Translator != nullptr;
}

void LanguageWorkflow::clearTranslator()
{
    if (!m_Translator) {
        return;
    }

    QCoreApplication::removeTranslator(m_Translator.get());
    m_Translator.reset();
}
