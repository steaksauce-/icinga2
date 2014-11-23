/******************************************************************************
 * Icinga 2                                                                   *
 * Copyright (C) 2012-2014 Icinga Development Team (http://www.icinga.org)    *
 *                                                                            *
 * This program is free software; you can redistribute it and/or              *
 * modify it under the terms of the GNU General Public License                *
 * as published by the Free Software Foundation; either version 2             *
 * of the License, or (at your option) any later version.                     *
 *                                                                            *
 * This program is distributed in the hope that it will be useful,            *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with this program; if not, write to the Free Software Foundation     *
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.             *
 ******************************************************************************/

#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "config/i2-config.hpp"
#include "config/vmframe.hpp"
#include "base/debuginfo.hpp"
#include "base/array.hpp"
#include "base/dictionary.hpp"
#include "base/scriptfunction.hpp"
#include "base/configerror.hpp"
#include "base/convert.hpp"
#include <boost/foreach.hpp>
#include <map>

namespace icinga
{

struct DebugHint
{
public:
	DebugHint(const Dictionary::Ptr& hints = Dictionary::Ptr())
		: m_Hints(hints)
	{ }

	inline void AddMessage(const String& message, const DebugInfo& di)
	{
		if (!m_Hints)
			m_Hints = new Dictionary();

		if (!m_Messages) {
			m_Messages = new Array();
			m_Hints->Set("messages", m_Messages);
		}

		Array::Ptr amsg = new Array();
		amsg->Add(message);
		amsg->Add(di.Path);
		amsg->Add(di.FirstLine);
		amsg->Add(di.FirstColumn);
		amsg->Add(di.LastLine);
		amsg->Add(di.LastColumn);
		m_Messages->Add(amsg);
	}

	inline DebugHint GetChild(const String& name)
	{
		if (!m_Hints)
			m_Hints = new Dictionary();

		if (!m_Children) {
			m_Children = new Dictionary;
			m_Hints->Set("properties", m_Children);
		}

		Dictionary::Ptr child = m_Children->Get(name);

		if (!child) {
			child = new Dictionary();
			m_Children->Set(name, child);
		}

		return DebugHint(child);
	}

	Dictionary::Ptr ToDictionary(void) const
	{
		return m_Hints;
	}

private:
	Dictionary::Ptr m_Hints;
	Array::Ptr m_Messages;
	Dictionary::Ptr m_Children;
};

enum CombinedSetOp
{
	OpSetLiteral,
	OpSetAdd,
	OpSetSubtract,
	OpSetMultiply,
	OpSetDivide
};

class InterruptExecutionError : virtual public std::exception, virtual public boost::exception
{
public:
	InterruptExecutionError(const Value& result)
		: m_Result(result)
	{ }

	~InterruptExecutionError(void) throw()
	{ }

	Value GetResult(void) const
	{
		return m_Result;
	}

private:
	Value m_Result;
};

typedef std::map<String, String> DefinitionMap;

/**
 * @ingroup config
 */
class I2_CONFIG_API Expression
{
public:
	virtual ~Expression(void);

	Value Evaluate(VMFrame& frame, DebugHint *dhint = NULL) const;

	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const = 0;
	virtual const DebugInfo& GetDebugInfo(void) const;
};

I2_CONFIG_API std::vector<Expression *> MakeIndexer(const String& index1);

class I2_CONFIG_API OwnedExpression : public Expression
{
public:
	OwnedExpression(const boost::shared_ptr<Expression>& expression)
		: m_Expression(expression)
	{ }

protected:
	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const
	{
		return m_Expression->DoEvaluate(frame, dhint);
	}

	virtual const DebugInfo& GetDebugInfo(void) const
	{
		return m_Expression->GetDebugInfo();
	}

private:
	boost::shared_ptr<Expression> m_Expression;
};

class I2_CONFIG_API LiteralExpression : public Expression
{
public:
	LiteralExpression(const Value& value = Value());

protected:
	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const;

private:
	Value m_Value;
};

inline LiteralExpression *MakeLiteral(const Value& literal = Value())
{
	return new LiteralExpression(literal);
}

class I2_CONFIG_API DebuggableExpression : public Expression
{
public:
	DebuggableExpression(const DebugInfo& debugInfo = DebugInfo())
		: m_DebugInfo(debugInfo)
	{ }

protected:
	virtual const DebugInfo& GetDebugInfo(void) const;

	DebugInfo m_DebugInfo;
};

class I2_CONFIG_API UnaryExpression : public DebuggableExpression
{
public:
	UnaryExpression(Expression *operand, const DebugInfo& debugInfo = DebugInfo())
		: DebuggableExpression(debugInfo), m_Operand(operand)
	{ }

	~UnaryExpression(void)
	{
		delete m_Operand;
	}

protected:
	Expression *m_Operand;
};

class I2_CONFIG_API BinaryExpression : public DebuggableExpression
{
public:
	BinaryExpression(Expression *operand1, Expression *operand2, const DebugInfo& debugInfo = DebugInfo())
		: DebuggableExpression(debugInfo), m_Operand1(operand1), m_Operand2(operand2)
	{ }
	
	~BinaryExpression(void)
	{
		delete m_Operand1;
		delete m_Operand2;
	}

protected:
	Expression *m_Operand1;
	Expression *m_Operand2;
};

	
class I2_CONFIG_API VariableExpression : public DebuggableExpression
{
public:
	VariableExpression(const String& variable, const DebugInfo& debugInfo = DebugInfo())
		: DebuggableExpression(debugInfo), m_Variable(variable)
	{ }

	String GetVariable(void) const
	{
		return m_Variable;
	}

protected:
	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const;

private:
	String m_Variable;
};
	
class I2_CONFIG_API NegateExpression : public UnaryExpression
{
public:
	NegateExpression(Expression *operand, const DebugInfo& debugInfo = DebugInfo())
		: UnaryExpression(operand, debugInfo)
	{ }

protected:
	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const;
};
	
class I2_CONFIG_API LogicalNegateExpression : public UnaryExpression
{
public:
	LogicalNegateExpression(Expression *operand, const DebugInfo& debugInfo = DebugInfo())
		: UnaryExpression(operand, debugInfo)
	{ }

protected:
	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const;
};

class I2_CONFIG_API AddExpression : public BinaryExpression
{
public:
	AddExpression(Expression *operand1, Expression *operand2, const DebugInfo& debugInfo = DebugInfo())
		: BinaryExpression(operand1, operand2, debugInfo)
	{ }

protected:
	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const;
};
	
class I2_CONFIG_API SubtractExpression : public BinaryExpression
{
public:
	SubtractExpression(Expression *operand1, Expression *operand2, const DebugInfo& debugInfo = DebugInfo())
		: BinaryExpression(operand1, operand2, debugInfo)
	{ }

protected:
	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const;
};
	
class I2_CONFIG_API MultiplyExpression : public BinaryExpression
{
public:
	MultiplyExpression(Expression *operand1, Expression *operand2, const DebugInfo& debugInfo = DebugInfo())
		: BinaryExpression(operand1, operand2, debugInfo)
	{ }

protected:
	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const;
};
	
class I2_CONFIG_API DivideExpression : public BinaryExpression
{
public:
	DivideExpression(Expression *operand1, Expression *operand2, const DebugInfo& debugInfo = DebugInfo())
		: BinaryExpression(operand1, operand2, debugInfo)
	{ }

protected:
	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const;
};
	
class I2_CONFIG_API BinaryAndExpression : public BinaryExpression
{
public:
	BinaryAndExpression(Expression *operand1, Expression *operand2, const DebugInfo& debugInfo = DebugInfo())
		: BinaryExpression(operand1, operand2, debugInfo)
	{ }

protected:
	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const;
};
	
class I2_CONFIG_API BinaryOrExpression : public BinaryExpression
{
public:
	BinaryOrExpression(Expression *operand1, Expression *operand2, const DebugInfo& debugInfo = DebugInfo())
		: BinaryExpression(operand1, operand2, debugInfo)
	{ }

protected:
	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const;
};
	
class I2_CONFIG_API ShiftLeftExpression : public BinaryExpression
{
public:
	ShiftLeftExpression(Expression *operand1, Expression *operand2, const DebugInfo& debugInfo = DebugInfo())
		: BinaryExpression(operand1, operand2, debugInfo)
	{ }

protected:
	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const;
};
	
class I2_CONFIG_API ShiftRightExpression : public BinaryExpression
{
public:
	ShiftRightExpression(Expression *operand1, Expression *operand2, const DebugInfo& debugInfo = DebugInfo())
		: BinaryExpression(operand1, operand2, debugInfo)
	{ }

protected:
	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const;
};
	
class I2_CONFIG_API EqualExpression : public BinaryExpression
{
public:
	EqualExpression(Expression *operand1, Expression *operand2, const DebugInfo& debugInfo = DebugInfo())
		: BinaryExpression(operand1, operand2, debugInfo)
	{ }

protected:
	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const;
};
	
class I2_CONFIG_API NotEqualExpression : public BinaryExpression
{
public:
	NotEqualExpression(Expression *operand1, Expression *operand2, const DebugInfo& debugInfo = DebugInfo())
		: BinaryExpression(operand1, operand2, debugInfo)
	{ }

protected:
	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const;
};
	
class I2_CONFIG_API LessThanExpression : public BinaryExpression
{
public:
	LessThanExpression(Expression *operand1, Expression *operand2, const DebugInfo& debugInfo = DebugInfo())
		: BinaryExpression(operand1, operand2, debugInfo)
	{ }

protected:
	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const;
};
	
class I2_CONFIG_API GreaterThanExpression : public BinaryExpression
{
public:
	GreaterThanExpression(Expression *operand1, Expression *operand2, const DebugInfo& debugInfo = DebugInfo())
		: BinaryExpression(operand1, operand2, debugInfo)
	{ }

protected:
	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const;
};
	
class I2_CONFIG_API LessThanOrEqualExpression : public BinaryExpression
{
public:
	LessThanOrEqualExpression(Expression *operand1, Expression *operand2, const DebugInfo& debugInfo = DebugInfo())
		: BinaryExpression(operand1, operand2, debugInfo)
	{ }

protected:
	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const;
};
	
class I2_CONFIG_API GreaterThanOrEqualExpression : public BinaryExpression
{
public:
	GreaterThanOrEqualExpression(Expression *operand1, Expression *operand2, const DebugInfo& debugInfo = DebugInfo())
		: BinaryExpression(operand1, operand2, debugInfo)
	{ }

protected:
	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const;
};
	
class I2_CONFIG_API InExpression : public BinaryExpression
{
public:
	InExpression(Expression *operand1, Expression *operand2, const DebugInfo& debugInfo = DebugInfo())
		: BinaryExpression(operand1, operand2, debugInfo)
	{ }

protected:
	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const;
};
	
class I2_CONFIG_API NotInExpression : public BinaryExpression
{
public:
	NotInExpression(Expression *operand1, Expression *operand2, const DebugInfo& debugInfo = DebugInfo())
		: BinaryExpression(operand1, operand2, debugInfo)
	{ }

protected:
	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const;
};
	
class I2_CONFIG_API LogicalAndExpression : public BinaryExpression
{
public:
	LogicalAndExpression(Expression *operand1, Expression *operand2, const DebugInfo& debugInfo = DebugInfo())
		: BinaryExpression(operand1, operand2, debugInfo)
	{ }

protected:
	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const;
};
	
class I2_CONFIG_API LogicalOrExpression : public BinaryExpression
{
public:
	LogicalOrExpression(Expression *operand1, Expression *operand2, const DebugInfo& debugInfo = DebugInfo())
		: BinaryExpression(operand1, operand2, debugInfo)
	{ }

protected:
	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const;
};
	
class I2_CONFIG_API FunctionCallExpression : public DebuggableExpression
{
public:
	FunctionCallExpression(Expression *fname, const std::vector<Expression *>& args, const DebugInfo& debugInfo = DebugInfo())
		: DebuggableExpression(debugInfo), m_FName(fname), m_Args(args)
	{ }

	~FunctionCallExpression(void)
	{
		delete m_FName;

		BOOST_FOREACH(Expression *expr, m_Args)
			delete expr;
	}

protected:
	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const;

public:
	Expression *m_FName;
	std::vector<Expression *> m_Args;
};
	
class I2_CONFIG_API ArrayExpression : public DebuggableExpression
{
public:
	ArrayExpression(const std::vector<Expression *>& expressions, const DebugInfo& debugInfo = DebugInfo())
		: DebuggableExpression(debugInfo), m_Expressions(expressions)
	{ }

	~ArrayExpression(void)
	{
		BOOST_FOREACH(Expression *expr, m_Expressions)
			delete expr;
	}

protected:
	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const;

private:
	std::vector<Expression *> m_Expressions;
};
	
class I2_CONFIG_API DictExpression : public DebuggableExpression
{
public:
	DictExpression(const std::vector<Expression *>& expressions, const DebugInfo& debugInfo = DebugInfo())
		: DebuggableExpression(debugInfo), m_Expressions(expressions), m_Inline(false)
	{ }

	~DictExpression(void)
	{
		BOOST_FOREACH(Expression *expr, m_Expressions)
			delete expr;
	}

	void MakeInline(void);

protected:
	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const;

private:
	std::vector<Expression *> m_Expressions;
	bool m_Inline;
};
	
class I2_CONFIG_API SetExpression : public DebuggableExpression
{
public:
	SetExpression(const std::vector<Expression *>& indexer, CombinedSetOp op, Expression *operand2, bool local, const DebugInfo& debugInfo = DebugInfo())
		: DebuggableExpression(debugInfo), m_Op(op), m_Indexer(indexer), m_Operand2(operand2), m_Local(local)
	{ }

	~SetExpression(void)
	{
		BOOST_FOREACH(Expression *expr, m_Indexer)
			delete expr;

		delete m_Operand2;
	}

protected:
	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const;

private:
	CombinedSetOp m_Op;
	std::vector<Expression *> m_Indexer;
	Expression *m_Operand2;
	bool m_Local;

};

class I2_CONFIG_API ReturnExpression : public UnaryExpression
{
public:
	ReturnExpression(Expression *expression, const DebugInfo& debugInfo = DebugInfo())
		: UnaryExpression(expression, debugInfo)
	{ }

protected:
	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const;
};

class I2_CONFIG_API IndexerExpression : public DebuggableExpression
{
public:
	IndexerExpression(const std::vector<Expression *>& indexer, const DebugInfo& debugInfo = DebugInfo())
		: DebuggableExpression(debugInfo), m_Indexer(indexer)
	{ }

protected:
	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const;

private:
	std::vector<Expression *> m_Indexer;
};

class I2_CONFIG_API ImportExpression : public DebuggableExpression
{
public:
	ImportExpression(Expression *name, const DebugInfo& debugInfo = DebugInfo())
		: DebuggableExpression(debugInfo), m_Name(name)
	{ }

	~ImportExpression(void)
	{
		delete m_Name;
	}

protected:
	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const;

private:
	Expression *m_Name;
};

class I2_CONFIG_API FunctionExpression : public DebuggableExpression
{
public:
	FunctionExpression(const String& name, const std::vector<String>& args,
	    std::map<String, Expression *> *closedVars, Expression *expression, const DebugInfo& debugInfo = DebugInfo())
		: DebuggableExpression(debugInfo), m_Name(name), m_Args(args), m_ClosedVars(closedVars), m_Expression(expression)
	{ }

protected:
	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const;

private:
	String m_Name;
	std::vector<String> m_Args;
	std::map<String, Expression *> *m_ClosedVars;
	boost::shared_ptr<Expression> m_Expression;
};

class I2_CONFIG_API SlotExpression : public DebuggableExpression
{
public:
	SlotExpression(const String& signal, Expression *slot, const DebugInfo& debugInfo = DebugInfo())
		: DebuggableExpression(debugInfo), m_Signal(signal), m_Slot(slot)
	{ }

protected:
	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const;

private:
	String m_Signal;
	Expression *m_Slot;
};

class I2_CONFIG_API ApplyExpression : public DebuggableExpression
{
public:
	ApplyExpression(const String& type, const String& target, Expression *name,
	    Expression *filter, const String& fkvar, const String& fvvar,
	    Expression *fterm, std::map<String, Expression *> *closedVars,
	    Expression *expression, const DebugInfo& debugInfo = DebugInfo())
		: DebuggableExpression(debugInfo), m_Type(type), m_Target(target),
		    m_Name(name), m_Filter(filter), m_FKVar(fkvar), m_FVVar(fvvar),
		    m_FTerm(fterm), m_ClosedVars(closedVars), m_Expression(expression)
	{ }

	~ApplyExpression(void)
	{
		delete m_Name;
	}

protected:
	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const;

private:
	String m_Type;
	String m_Target;
	Expression *m_Name;
	boost::shared_ptr<Expression> m_Filter;
	String m_FKVar;
	String m_FVVar;
	boost::shared_ptr<Expression> m_FTerm;
	std::map<String, Expression *> *m_ClosedVars;
	boost::shared_ptr<Expression> m_Expression;
};

class I2_CONFIG_API ObjectExpression : public DebuggableExpression
{
public:
	ObjectExpression(bool abstract, const String& type, Expression *name, Expression *filter,
	    const String& zone, std::map<String, Expression *> *closedVars,
	    Expression *expression, const DebugInfo& debugInfo = DebugInfo())
		: DebuggableExpression(debugInfo), m_Abstract(abstract), m_Type(type),
		  m_Name(name), m_Filter(filter), m_Zone(zone), m_ClosedVars(closedVars), m_Expression(expression)
	{ }

	~ObjectExpression(void)
	{
		delete m_Name;
	}

protected:
	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const;

private:
	bool m_Abstract;
	String m_Type;
	Expression *m_Name;
	boost::shared_ptr<Expression> m_Filter;
	String m_Zone;
	std::map<String, Expression *> *m_ClosedVars;
	boost::shared_ptr<Expression> m_Expression;
};
	
class I2_CONFIG_API ForExpression : public DebuggableExpression
{
public:
	ForExpression(const String& fkvar, const String& fvvar, Expression *value, Expression *expression, const DebugInfo& debugInfo = DebugInfo())
		: DebuggableExpression(debugInfo), m_FKVar(fkvar), m_FVVar(fvvar), m_Value(value), m_Expression(expression)
	{ }

	~ForExpression(void)
	{
		delete m_Value;
		delete m_Expression;
	}

protected:
	virtual Value DoEvaluate(VMFrame& frame, DebugHint *dhint) const;

private:
	String m_FKVar;
	String m_FVVar;
	Expression *m_Value;
	Expression *m_Expression;
};

}

#endif /* EXPRESSION_H */
