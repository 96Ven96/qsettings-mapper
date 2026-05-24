#pragma once

#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

#include <QSettings>
#include <QString>
#include <QVariant>

#include "templatedefines.h"

/**
 * @brief Declarative QSettings serialization helpers.
 *
 * Build a compile-time description of how a class maps onto a QSettings
 * tree (via getter/setter pairs or direct member-pointer access, with
 * optional defaults and nested groups), then call
 * @code SettingsHelper::save<MyGroup>(settings, obj); @endcode
 * or
 * @code SettingsHelper::load<MyGroup>(settings, obj); @endcode
 * to (de)serialize the whole hierarchy in one go.
 */
namespace SettingsHelperNs {

    /// @brief Marker type used to signal "no default value".
    using NoDefaultType = std::nullptr_t;

    /// @brief Sentinel used as the default template argument for "no default".
    static constexpr NoDefaultType NoDefault = nullptr;

    // -------------------------------------------------------------------------
    // SettingsGS — getter/setter-based item
    // -------------------------------------------------------------------------

    /**
     * @brief Maps a single field of @p RefClass to a QSettings key via a
     *        getter and a setter member function.
     *
     * @tparam RefClass The class being (de)serialized.
     * @tparam Getter_  Member function pointer used to read the field.
     * @tparam Setter_  Member function pointer used to write the field.
     * @tparam KeyStr_  Key under which the value is stored (string literal or
     *                  std::array<char,N>).
     * @tparam Default_ Optional default value applied when the key is missing.
     *                  Can be either a value or a nullary callable returning
     *                  the value.
     */
    template <typename RefClass, auto Getter_, auto Setter_, auto KeyStr_,
              auto Default_ = NoDefault>
    struct SettingsGS {
        using GetterT = decltype(Getter_);
        using SetterT = decltype(Setter_);
        using VarT    = FunctionTraitsNs::remove_cvref_t<std::invoke_result_t<GetterT, RefClass>>;

        static_assert(!std::is_same_v<VarT, void> && !std::is_pointer_v<VarT>,
                      "Invalid getter return type");
        static_assert(std::is_invocable_v<SetterT, RefClass, VarT>,
                      "Getter and setter types are incoherent");

        static constexpr GetterT Getter     = Getter_;
        static constexpr SetterT Setter     = Setter_;
        static constexpr auto    KeyStr     = KeyStr_;
        static constexpr bool    HasDefault = !std::is_same_v<decltype(Default_), NoDefaultType>;
        static constexpr auto    DefaultVal = Default_;

        [[nodiscard]] static decltype(auto) val(const RefClass& obj) noexcept {
            return (std::invoke(Getter_, obj));
        }

        template <typename ValT>
        static void setVal(RefClass& obj, ValT&& val) noexcept {
            // TODO: relax to std::is_convertible_v when needed.
            static_assert(std::is_same_v<FunctionTraitsNs::remove_cvref_t<ValT>, VarT>,
                          "Invalid input value type");
            std::invoke(Setter_, obj, std::forward<ValT>(val));
        }
    };

    // -------------------------------------------------------------------------
    // SettingsVar — direct member-pointer item
    // -------------------------------------------------------------------------

    /**
     * @brief Maps a single member variable of @p RefClass to a QSettings key.
     *
     * Same template parameters as SettingsGS, but uses a direct member-data
     * pointer instead of getter/setter functions.
     */
    template <typename RefClass, auto InstanceVar_, auto KeyStr_,
              auto Default_ = NoDefault>
    struct SettingsVar {
        static constexpr auto InstanceVar = InstanceVar_;
        static constexpr auto DefaultVal  = Default_;
        static constexpr auto KeyStr      = KeyStr_;
        static constexpr bool HasDefault  = !std::is_same_v<decltype(Default_), NoDefaultType>;
        using VarT = FunctionTraitsNs::remove_cvref_t<decltype(std::declval<RefClass>().*InstanceVar_)>;

        static_assert(!std::is_same_v<VarT, void> && !std::is_pointer_v<VarT>,
                      "Invalid member type");

        [[nodiscard]] static decltype(auto) val(const RefClass& obj) noexcept {
            return (obj.*InstanceVar_);
        }

        template <typename ValT>
        static void setVal(RefClass& obj, ValT&& val) noexcept {
            // TODO: relax to std::is_convertible_v when needed.
            static_assert(std::is_same_v<FunctionTraitsNs::remove_cvref_t<ValT>, VarT>,
                          "Invalid input value type");
            obj.*InstanceVar_ = std::forward<ValT>(val);
        }
    };

    // -------------------------------------------------------------------------
    // Traits — runtime/compile-time identification of item kinds
    // -------------------------------------------------------------------------

    namespace Traits {

        template <typename T>
        struct is_settings_gs : std::false_type {};
        template <typename R, auto G, auto S, auto K, auto D>
        struct is_settings_gs<SettingsGS<R, G, S, K, D>> : std::true_type {};
        template <typename T>
        inline constexpr bool is_settings_gs_v = is_settings_gs<T>::value;

        template <typename T>
        struct is_settings_var : std::false_type {};
        template <typename R, auto V, auto K, auto D>
        struct is_settings_var<SettingsVar<R, V, K, D>> : std::true_type {};
        template <typename T>
        inline constexpr bool is_settings_var_v = is_settings_var<T>::value;

        template <typename T>
        inline constexpr bool is_settings_v = is_settings_gs_v<T> || is_settings_var_v<T>;

        template <typename T> struct is_group : std::false_type {};
        // specialized below, after Group's declaration.
        template <typename T> inline constexpr bool is_group_v = is_group<T>::value;

    } // namespace Traits

    // -------------------------------------------------------------------------
    // Group — recursive container of items and sub-groups
    // -------------------------------------------------------------------------

    /**
     * @brief A named group of settings, possibly nested.
     *
     * Each item must be either a SettingsGS, a SettingsVar, or another Group
     * (which produces a nested QSettings key path via beginGroup / endGroup).
     */
    template <typename RefClass, auto KeyStr_, typename... Items>
    struct Group {
        static_assert(((Traits::is_settings_v<Items> || Traits::is_group_v<Items>) && ...),
                      "Every item must be a SettingsGS, a SettingsVar, or a Group");
        using ClassT     = RefClass;
        using ItemsTuple = std::tuple<Items...>;
        static constexpr auto KeyStr = KeyStr_;
    };

    template <typename R, auto K, typename... I>
    struct Traits::is_group<Group<R, K, I...>> : std::true_type {};

} // namespace SettingsHelperNs


/*
 * // ----------------------------------------- //
 * //               EXAMPLE                     //
 * // ----------------------------------------- //
 *
 * using namespace SettingsHelperNs;
 *
 * using MySettings = Group<MyClass, "MyClass",
 *     SettingsGS<MyClass, &MyClass::name,  &MyClass::setName,  "name">,
 *     SettingsGS<MyClass, &MyClass::value, &MyClass::setValue, "value", 42>,
 *     Group<MyClass, "advanced",
 *         SettingsVar<MyClass, &MyClass::m_flag, "flag", false>
 *     >
 * >;
 *
 * QSettings settings;
 * MyClass   obj;
 *
 * SettingsHelper::save<MySettings>(settings, obj);
 * SettingsHelper::load<MySettings>(settings, obj);
 */

/**
 * @brief Drives save/load over a SettingsHelperNs::Group description.
 *
 * The class is non-instantiable — all entry points are static templates.
 * Pick a Group description (with @c using ) and call
 * @c SettingsHelper::save<MyGroup>(settings, obj) /
 * @c SettingsHelper::load<MyGroup>(settings, obj) .
 */
class SettingsHelper
{
    /// @brief RAII helper that pushes a group on construction and pops it on destruction.
    struct GroupGuard {
        QSettings& settings;
        GroupGuard(QSettings& settings_, const char* key) : settings{settings_} {
            settings.beginGroup(key);
        }
        template <std::size_t N>
        GroupGuard(QSettings& settings_, const std::array<char, N>& key) : settings{settings_} {
            settings.beginGroup(key.data());
        }
        ~GroupGuard() { settings.endGroup(); }
    };

    template <typename Item, typename ObjT>
    static void saveItem(QSettings& settings, const ObjT& obj) noexcept {
        namespace Traits = SettingsHelperNs::Traits;
        if constexpr (Traits::is_group_v<Item>) {
            GroupGuard guard(settings, Item::KeyStr);
            saveImpl<Item>(settings, obj);
        } else {
            using VarT = typename Item::VarT;
            const VarT& val = Item::val(obj);
            settings.setValue(Item::KeyStr, ConversionTypesNs::Qt_::toVariant(val));
        }
    }

#if !(__cplusplus >= 202002L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L))
    // C++17 fallback: no template lambdas yet, so we need explicit helpers.
    template <typename GroupT, typename Tuple, std::size_t... Is>
    static void saveUnpack(QSettings& settings, const typename GroupT::ClassT& obj,
                            std::index_sequence<Is...>) noexcept {
        (saveItem<std::tuple_element_t<Is, Tuple>>(settings, obj), ...);
    }
    template <typename GroupT, typename Tuple, std::size_t... Is>
    static void loadUnpack(QSettings& settings, typename GroupT::ClassT& obj,
                            std::index_sequence<Is...>) noexcept {
        (loadItem<std::tuple_element_t<Is, Tuple>>(settings, obj), ...);
    }
#endif

    template <typename GroupT>
    static void saveImpl(QSettings& settings, const typename GroupT::ClassT& obj) noexcept {
        using Tuple = typename GroupT::ItemsTuple;
#if __cplusplus >= 202002L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            (saveItem<std::tuple_element_t<Is, Tuple>>(settings, obj), ...);
        }(std::make_index_sequence<std::tuple_size_v<Tuple>>{});
#else
        saveUnpack<GroupT, Tuple>(settings, obj, std::make_index_sequence<std::tuple_size_v<Tuple>>{});
#endif
    }

    template <typename GroupT>
    static void loadImpl(QSettings& settings, typename GroupT::ClassT& obj) noexcept {
        using Tuple = typename GroupT::ItemsTuple;
#if __cplusplus >= 202002L || (defined(_MSVC_LANG) && _MSVC_LANG >= 202002L)
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            (loadItem<std::tuple_element_t<Is, Tuple>>(settings, obj), ...);
        }(std::make_index_sequence<std::tuple_size_v<Tuple>>{});
#else
        loadUnpack<GroupT, Tuple>(settings, obj, std::make_index_sequence<std::tuple_size_v<Tuple>>{});
#endif
    }

    /**
     * @brief Resolves @c Item::DefaultVal — either invokes it (if it is a
     *        callable) or returns it directly.
     */
    template <typename Item>
    [[nodiscard]] static constexpr decltype(auto) defaultValue() noexcept {
        constexpr bool DefaultAsCallable = std::is_invocable_v<decltype(Item::DefaultVal)>;
        if constexpr (DefaultAsCallable) {
            return (std::invoke(Item::DefaultVal));
        } else {
            return Item::DefaultVal;
        }
    }

    template <typename Item, typename ObjT>
    static void applyDefault(ObjT& obj) noexcept {
        static_assert(Item::HasDefault, "applyDefault called on an item with no declared default");
        using VarT = typename Item::VarT;
        if constexpr (std::is_same_v<VarT, QString>) {
            // Allow C-string / std::string defaults to be applied to QString fields.
            Item::setVal(obj, ConversionTypesNs::Qt_::toQString(defaultValue<Item>()));
        } else {
            Item::setVal(obj, defaultValue<Item>());
        }
    }

    template <typename Item, typename ObjT>
    static void loadItem(QSettings& settings, ObjT& obj) noexcept {
        namespace Traits = SettingsHelperNs::Traits;
        if constexpr (Traits::is_group_v<Item>) {
            GroupGuard guard(settings, Item::KeyStr);
            loadImpl<Item>(settings, obj);
        } else {
            const QVariant raw = settings.value(Item::KeyStr);
            using VarT = typename Item::VarT;
            const auto valOpt = ConversionTypesNs::Qt_::fromVariant<VarT>(raw);
            if (valOpt.has_value()) {
                Item::setVal(obj, std::move(*valOpt));
            } else if constexpr (Item::HasDefault) {
                applyDefault<Item>(obj);
            } else {
                qWarning() << "SettingsHelper: failed to load key" << Item::KeyStr;
            }
        }
    }

public:

    /// @brief Serialize @p obj into @p settings under the path described by @c GroupT.
    template <typename GroupT>
    static void save(QSettings& settings, const typename GroupT::ClassT& obj) noexcept {
        GroupGuard guard(settings, GroupT::KeyStr);
        saveImpl<GroupT>(settings, obj);
    }

    /// @brief Deserialize @p obj from @p settings under the path described by @c GroupT.
    template <typename GroupT>
    static void load(QSettings& settings, typename GroupT::ClassT& obj) noexcept {
        GroupGuard guard(settings, GroupT::KeyStr);
        loadImpl<GroupT>(settings, obj);
    }

    SettingsHelper() = delete;
};
