
#include <ironbeepp/rule.hpp>

#include <ironbeepp/engine.hpp>
#include <ironbeepp/context.hpp>
#include <ironbeepp/var.hpp>

#include <ironbee/rule_engine.h>

namespace IronBee {

// ConstRule
ConstRule::ConstRule(): m_ib(NULL) {}

ConstRule::ConstRule(ib_type ib): m_ib(ib) {}

ConstVarExpand ConstRule::msg() const {
    return ConstVarExpand(ib()->meta.msg);
}

const char * ConstRule::rule_id() const
{
    return ib()->meta.id;
}

const char * ConstRule::full_rule_id() const
{
    return ib()->meta.full_id;
}

const char * ConstRule::chain_rule_id() const
{
    return ib()->meta.chain_id;
}

// Rule

Rule::Rule(): ConstRule(), m_ib(NULL) {}

Rule::Rule(ib_type ib): ConstRule(ib), m_ib(ib) {}

Rule Rule::remove_const(ConstRule rule)
{
    return Rule(const_cast<ib_type>(rule.ib()));
}

Rule Rule::lookup(
    Engine      engine,
    Context     context,
    const char* rule_id
) {
    ib_type ib;

    throw_if_error(
        ib_rule_lookup(engine.ib(), context.ib(), rule_id, &ib)
    );

    return Rule(ib);
}
} // namespace IronBee