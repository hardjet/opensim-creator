#pragma once

#include <oscar_document/NodePath.hpp>

#include <oscar/Utils/CStringView.hpp>

#include <cstddef>
#include <iosfwd>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace osc::doc { class PropertyDescriptions; }
namespace osc::doc { class Variant; }

namespace osc::doc
{
    class Node {
    protected:
        Node();
        Node(Node const&);
        Node(Node&&) noexcept;
        Node& operator=(Node const&);
        Node& operator=(Node&&) noexcept;
    public:
        virtual ~Node() noexcept;

        std::unique_ptr<Node> clone() const;

        CStringView getName() const;  // cannot contain special chars or be called "children"
        void setName(std::string_view);


        template<
            typename TDerived = Node,
            typename std::enable_if_t<std::is_base_of_v<Node, TDerived>, bool> = true
        >
        TDerived const* getParent() const
        {
            return dynamic_cast<TDerived const*>(getParent<Node>());
        }
        template<>
        Node const* getParent<Node>() const;

        template<
            typename TDerived = Node,
            typename std::enable_if_t<std::is_base_of_v<Node, TDerived>, bool> = true
        >
        TDerived* updParent()
        {
            return dynamic_cast<TDerived*>(updParent<Node>());
        }
        template<>
        Node* updParent<Node>();


        size_t getNumChildren() const;

        template<
            typename TDerived = Node,
            typename std::enable_if_t<std::is_base_of_v<Node, TDerived>, bool> = true
        >
        TDerived const* getChild(size_t i) const
        {
            return dynamic_cast<TDerived const*>(getChild<Node>());
        }
        template<>
        Node const* getChild<Node>(size_t) const;

        template<
            typename TDerived = Node,
            typename std::enable_if_t<std::is_base_of_v<Node, TDerived>, bool> = true
        >
        TDerived const* getChild(std::string_view childName) const
        {
            return dynamic_cast<TDerived const*>(getChild<Node>(childName));
        }
        template<>
        Node const* getChild<Node>(std::string_view) const;

        template<
            typename TDerived = Node,
            typename std::enable_if_t<std::is_base_of_v<Node, TDerived>, bool> = true
        >
        TDerived* updChild(size_t i)
        {
            return dynamic_cast<TDerived*>(updChild<Node>(i));
        }
        template<>
        Node* updChild(size_t);

        template<
            typename TDerived = Node,
            typename std::enable_if_t<std::is_base_of_v<Node, TDerived>, bool> = true
        >
        TDerived* updChild(std::string_view childName)
        {
            return dynamic_cast<TDerived*>(updChild<Node>(childName));
        }
        template<>
        Node* updChild<Node>(std::string_view childName);

        template<typename TDerived>
        TDerived& addChild(std::unique_ptr<TDerived> p)
        {
            TDerived& rv = *p;
            addChild<Node>(std::move(p));
            return rv;
        }
        template<>
        Node& addChild<Node>(std::unique_ptr<Node>);

        bool removeChild(size_t);
        bool removeChild(Node&);
        bool removeChild(std::string_view childName);


        NodePath getAbsolutePath() const;

        template<
            typename TDerived = Node,
            typename std::enable_if_t<std::is_base_of_v<Node, TDerived>, bool> = true
        >
        TDerived const* find(NodePath const& p) const
        {
            return dynamic_cast<TDerived const*>(find<Node>(p));
        }
        template<>
        Node const* find<Node>(NodePath const&) const;

        template<
            typename TDerived = Node,
            typename std::enable_if_t<std::is_base_of_v<Node, TDerived>, bool> = true
        >
        TDerived* find(NodePath const& p)
        {
            return dynamic_cast<TDerived*>(find<Node>(p));
        }
        template<>
        Node* find<Node>(NodePath const&);

        size_t getNumProperties() const;
        bool hasProperty(std::string_view propName) const;
        CStringView getPropertyName(size_t) const;
        Variant const& getPropertyValue(size_t) const;
        Variant const* getPropertyValue(std::string_view propName) const;
        bool setPropertyValue(size_t, Variant const&);
        bool setPropertyValue(std::string_view propName, Variant const&);

    private:
        virtual std::unique_ptr<Node> implClone() const = 0;
        virtual PropertyDescriptions const& implGetPropertyList() const {}

        class Impl;
        std::shared_ptr<Impl> m_Impl;
    };
}