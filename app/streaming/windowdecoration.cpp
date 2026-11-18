#include "windowdecoration.h"

#ifdef Q_OS_LINUX
#include <dlfcn.h>
#include <sys/types.h>
#endif

namespace WindowDecoration {

#ifdef Q_OS_LINUX
bool styleClientSideHeader(bool dark)
{
    // libdecor loaded GTK privately, so these are only reachable by asking for
    // the copy already in this process. Starlight never loads GTK itself.
    void* gtk = dlopen("libgtk-3.so.0", RTLD_NOW | RTLD_NOLOAD);
    void* gdk = dlopen("libgdk-3.so.0", RTLD_NOW | RTLD_NOLOAD);
    if (!gtk || !gdk) return false;

    using ProviderNew = void* (*)();
    using LoadFromData = int (*)(void*, const char*, ssize_t, void**);
    using ScreenDefault = void* (*)();
    using AddProvider = void (*)(void*, void*, unsigned int);
    const auto providerNew = reinterpret_cast<ProviderNew>(dlsym(gtk, "gtk_css_provider_new"));
    const auto loadFromData = reinterpret_cast<LoadFromData>(dlsym(gtk, "gtk_css_provider_load_from_data"));
    const auto addProvider = reinterpret_cast<AddProvider>(dlsym(gtk, "gtk_style_context_add_provider_for_screen"));
    const auto screenDefault = reinterpret_cast<ScreenDefault>(dlsym(gdk, "gdk_screen_get_default"));
    if (!providerNew || !loadFromData || !addProvider || !screenDefault) return false;
    void* screen = screenDefault();
    if (!screen) return false;

    // One provider for the life of the process: loading replaces its rules, so
    // a desktop that changed theme between streams is followed.
    static void* provider = nullptr;
    const auto style = headerStyleSheet(dark);
    if (!provider) {
        provider = providerNew();
        if (!provider) return false;
        loadFromData(provider, style.constData(), style.size(), nullptr);
        constexpr unsigned int PriorityApplication = 600;
        addProvider(screen, provider, PriorityApplication);
    }
    else loadFromData(provider, style.constData(), style.size(), nullptr);
    return true;
}
#else
bool styleClientSideHeader(bool) { return false; }
#endif

}
