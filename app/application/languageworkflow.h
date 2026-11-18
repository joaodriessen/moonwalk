#pragma once

#include <QString>

#include <memory>

class QTranslator;

class LanguageWorkflow
{
public:
    enum LanguageCode
    {
        Automatic = 0,
        English,
        French,
        SimplifiedChinese,
        German,
        NorwegianBokmal,
        Russian,
        Spanish,
        Japanese,
        Vietnamese,
        Thai,
        Korean,
        Hungarian,
        Dutch,
        Swedish,
        Turkish,
        Ukrainian,
        TraditionalChinese,
        Portuguese,
        BrazilianPortuguese,
        Greek,
        Italian,
        Hindi,
        Polish,
        Czech,
        Hebrew,
        CentralKurdish,
        Lithuanian,
        Estonian,
        Bulgarian,
        Esperanto,
        Tamil,
        LastLanguage = Tamil,
    };

    enum class Status
    {
        ApplyNow,
        ApplyAfterRestart,
        Unchanged,
        InvalidInput,
    };

    struct Decision
    {
        Status status;
        int language;
        bool retranslate;
        bool clearOnBack;
        bool showRestartNotice;
    };

    class StatePort
    {
    public:
        virtual ~StatePort() = default;
        virtual int currentLanguage() const = 0;
        virtual void commitLanguage(int language) = 0;
    };

    LanguageWorkflow();
    ~LanguageWorkflow();

    LanguageWorkflow(const LanguageWorkflow&) = delete;
    LanguageWorkflow& operator=(const LanguageWorkflow&) = delete;

    static Decision planChange(int currentLanguage,
                               int requestedLanguage,
                               bool dynamicRetranslationSupported);
    static bool isValidLanguage(int language);
    static QString suffixForLanguage(int language, const QString& systemLocale);

    Decision changeLanguage(StatePort& state,
                            int requestedLanguage,
                            bool dynamicRetranslationSupported);
    bool applyTranslation(int language);
    bool translationLoaded() const;

private:
    void clearTranslator();

    std::unique_ptr<QTranslator> m_Translator;
};
