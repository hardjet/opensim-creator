#pragma once

#include <oscar/Platform/IResourceLoader.hpp>
#include <oscar/Platform/ResourcePath.hpp>
#include <oscar/Platform/ResourceStream.hpp>

#include <concepts>
#include <memory>
#include <string>
#include <utility>

namespace osc
{
    class ResourceLoader {
    public:
        ResourceStream open(ResourcePath const& p) { return m_Impl->open(m_Prefix / p); }
        std::string slurp(ResourcePath const&);
        ResourceLoader withPrefix(ResourcePath const& prefix) const { return ResourceLoader{m_Impl, m_Prefix / prefix}; }
        ResourceLoader withPrefix(std::string_view sv) { return withPrefix(ResourcePath{sv}); }
    private:
        template<
            std::derived_from<IResourceLoader> TResourceLoader,
            typename... Args
        >
        friend ResourceLoader make_resource_loader(Args&&...)
            requires std::constructible_from<TResourceLoader, Args&&...>;

        explicit ResourceLoader(
            std::shared_ptr<IResourceLoader> impl_,
            ResourcePath const& prefix_ = ResourcePath{}) :

            m_Impl{std::move(impl_)},
            m_Prefix{prefix_}
        {}

        std::shared_ptr<IResourceLoader> m_Impl;
        ResourcePath m_Prefix;
    };

    template<
        std::derived_from<IResourceLoader> TResourceLoader,
        typename... Args
    >
    ResourceLoader make_resource_loader(Args&&... args)
        requires std::constructible_from<TResourceLoader, Args&&...>
    {
        return ResourceLoader{std::make_shared<TResourceLoader>(std::forward<Args>(args)...)};
    }
}
