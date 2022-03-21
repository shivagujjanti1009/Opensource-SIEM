/* Copyright (C) 2015-2021, Wazuh Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */

#ifndef _CONNECTABLE_H
#define _CONNECTABLE_H

#include "json.hpp"
#include <rxcpp/rx.hpp>
#include <set>

namespace builder::internals
{

/**
 * @brief A connectable is a structure which help us build the RXCPP graph when
 * our assets are in the graph.
 *
 * @tparam Observable
 */
template<class Observable>
struct Connectable
{
    /**
     * @brief An operation is a function which will generate
     * an observable with the apropriate transformations and filters
     * generated by the asset building process.
     */
    using Op_t = std::function<Observable(const Observable &)>;
    Op_t m_op;

    /**
     * @brief Used to distinguish between connectables. Also for debugging
     * purposes. It is derived from the name of the asset it contains de
     * operation for.
     */
    std::string m_name;

    /**
     * @brief The name of the parents of this connectable, derived directly
     * from the assets definition.
     */
    std::set<std::string> m_parents;

    /**
     * @brief The parents' outputs are this connectable inputs. Each connecatble
     * must merge their parents' output as a single input for this connectable
     * operation.
     */
    std::vector<Observable> m_inputs {};

    /**
     * @brief Construct a new Connectable object from its components.
     *
     * @param n name of the connectable
     * @param p vector of parents names
     * @param o the operation this connectable must do to the input stream.
     */
    Connectable(std::string n, std::vector<std::string> p, Op_t o) : m_op(o), m_name(n), m_parents(p.begin(), p.end())
    {
    }

    /**
     * @brief Construct a new Connectable object just from its name. It will use
     * a default passthough operation which allow us to create nodes whose sole
     * purpose is to facilitate the graph construction.
     *
     * @param n name of the connectable
     */
    Connectable(std::string n)
        : m_name(n)
    {
        if (n.find("OUTPUT_") == std::string::npos)
        {
            m_op = [](Observable o) { return o; };
        }
        else
        {
            m_op = [](Observable o) { return o.distinct_until_changed(); };
        }
    };

    /**
     * @brief Default constructor, needed my map
     *
     */
    Connectable() : Connectable{"not_initialized"}
    {
    }

    /**
     * @brief Adds an input stream to this connectable
     *
     * @param obs input stream
     */
    void addInput(Observable obs)
    {
        m_inputs.push_back(obs);
    }
    /**
     * @brief Connects an input stream with the operation of this
     * connectable and returns an observable with the operation already
     * attached.
     *
     * @param input
     * @return Observable
     */
    Observable connect(const Observable &input)
    {
        return m_op(input);
    }

    /**
     * @brief Merge all inputs of this node into a single stream and return
     * an obserbable with the operation already attached.
     *
     * @return Observable
     */
    Observable connect()
    {
        if (m_inputs.size() > 1)
        {
            return m_op(rxcpp::observable<>::iterate(m_inputs).merge());
        }
        return m_op(m_inputs[0]);
    }

    /**
     * @brief Operatos defined so Connectables can be stored on maps and sets as
     * keys.
     *
     */

    friend inline bool operator<(const Connectable &lhs, const Connectable &rhs)
    {
        return lhs.m_name < rhs.m_name;
    }

    friend inline std::ostream &operator<<(std::ostream &os,
                                           const Connectable &rhs)
    {
        os << rhs.m_name;
        return os;
    }

    friend inline bool operator!=(const Connectable &l, const Connectable &r)
    {
        return l.m_name != r.m_name;
    }

    friend inline bool operator==(const Connectable &l, const Connectable &r)
    {
        return l.m_name == r.m_name;
    }
};
} // namespace builder::internals
#endif // _CONNECTABLE_H
