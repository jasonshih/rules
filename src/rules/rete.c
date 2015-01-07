
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rules.h"
#include "net.h"
#include "json.h"

#define HASH_ALL 193486302 // all
#define HASH_ANY 193486381 // any
#define HASH_PRI 193502832 // pri
#define HASH_LT 193419881 // $lt
#define HASH_LTE 2087888878 // $lte
#define HASH_GT 193419716 // $gt
#define HASH_GTE 2087883433 // $gte
#define HASH_EQ 193419647 // $eq
#define HASH_NEQ 2087890573 // $neq
#define HASH_EX 193419654 // $ex
#define HASH_NEX 2087890580 // $nex
#define HASH_OR 193419978 // $or
#define HASH_AND 2087876700 // $and
#define HASH_S 5861212 // $s
#define HASH_NAME 2090536006 // name 
#define HASH_COUNT 1662456754 // $count
#define HASH_ADD 2087876370 // $add
#define HASH_SUB 2087896531 // $sub
#define HASH_MUL 2087890007 // $mul
#define HASH_DIV 2087879820 // $div
#define HASH_L 5861205 // $l
#define HASH_R 5861211 //$r

typedef struct path {
    unsigned char operator;
    struct path **parents;
    unsigned int parentsLength;
    unsigned short count;
    expression *expressions; 
    unsigned int expressionsLength;
} path;

typedef struct any {
    struct all **nodes;
    unsigned int nodesLength;
} any;

typedef struct all {
    unsigned short count;
    unsigned int *expressions;
    unsigned short expressionsLength;
} all;

static unsigned int validateAlgebra(char *rule);

static unsigned int createBeta(ruleset *tree, 
                               char *rule, 
                               unsigned char operator, 
                               unsigned int nextOffset, 
                               path *nextPath, 
                               unsigned short count,
                               path **outPath);

static unsigned int storeString(ruleset *tree, 
                                char *newString, 
                                unsigned int *stringOffset, 
                                unsigned int length) {

    unsigned int newStringLength = length + 1;
    if (!tree->stringPool) {
        tree->stringPool = malloc(newStringLength * sizeof(char));
        if (!tree->stringPool) {
            return ERR_OUT_OF_MEMORY;
        }

        *stringOffset = 0;
        tree->stringPoolLength = newStringLength;
    } else {
        tree->stringPool = realloc(tree->stringPool, (tree->stringPoolLength + newStringLength) * sizeof(char));
        if (!tree->stringPool) {
            return ERR_OUT_OF_MEMORY;
        }

        *stringOffset = tree->stringPoolLength;
        tree->stringPoolLength = tree->stringPoolLength + newStringLength;
    }

    strncpy(tree->stringPool + *stringOffset, newString, length);
    tree->stringPool[tree->stringPoolLength - 1] = '\0';
    return RULES_OK;
}

static unsigned int storeExpression(ruleset *tree, 
                                    expression **newExpression, 
                                    unsigned int *expressionOffset) {

    if (!tree->expressionPool) {
        tree->expressionPool = malloc(sizeof(expression));
        if (!tree->expressionPool) {
            return ERR_OUT_OF_MEMORY;
        }

        *expressionOffset = 0;
        *newExpression = &tree->expressionPool[0];
        tree->expressionOffset = 1;
    } else {
        tree->expressionPool = realloc(tree->expressionPool, (tree->expressionOffset + 1) * sizeof(expression));
        if (!tree->expressionPool) {
            return ERR_OUT_OF_MEMORY;
        }

        *expressionOffset = tree->expressionOffset;
        *newExpression = &tree->expressionPool[tree->expressionOffset];
        tree->expressionOffset = tree->expressionOffset + 1;        
    }

    return RULES_OK;
}

static unsigned int storeIdiom(ruleset *tree, 
                               idiom **newIdiom, 
                               unsigned int *idiomOffset) {

    if (!tree->idiomPool) {
        tree->idiomPool = malloc(sizeof(idiom));
        if (!tree->idiomPool) {
            return ERR_OUT_OF_MEMORY;
        }

        *idiomOffset = 0;
        *newIdiom = &tree->idiomPool[0];
        tree->idiomOffset = 1;
    } else {
        tree->idiomPool = realloc(tree->idiomPool, (tree->idiomOffset + 1) * sizeof(idiom));
        if (!tree->idiomPool) {
            return ERR_OUT_OF_MEMORY;
        }

        *idiomOffset = tree->idiomOffset;
        *newIdiom = &tree->idiomPool[tree->idiomOffset];
        tree->idiomOffset = tree->idiomOffset + 1;        
    }

    return RULES_OK;
}

static unsigned int appendTerm(expression *expr, unsigned int nodeOffset) {
    if (expr->termsLength == 0) {
        expr->termsLength = 1;
        expr->t.termsPointer = malloc(sizeof(unsigned int));
        if (!expr->t.termsPointer) {
            return ERR_OUT_OF_MEMORY;
        }

        expr->t.termsPointer[0] = nodeOffset;
    }
    else {
        expr->termsLength = expr->termsLength + 1;
        expr->t.termsPointer = realloc(expr->t.termsPointer, expr->termsLength * sizeof(unsigned int));
        if (!expr->t.termsPointer) {
            return ERR_OUT_OF_MEMORY;
        }

        expr->t.termsPointer[expr->termsLength - 1] = nodeOffset;
    }

    return RULES_OK;   
}

static unsigned int storeJoin(ruleset *tree, 
                              join **newJoin, 
                              unsigned int *joinOffset) {

    if (!tree->joinPool) {
        tree->joinPool = malloc(sizeof(join));
        if (!tree->joinPool) {
            return ERR_OUT_OF_MEMORY;
        }

        *joinOffset = 0;
        *newJoin = &tree->joinPool[0];
        tree->joinOffset = 1;
    } else {
        tree->joinPool = realloc(tree->joinPool, (tree->joinOffset + 1) * sizeof(join));
        if (!tree->joinPool) {
            return ERR_OUT_OF_MEMORY;
        }

        *joinOffset = tree->joinOffset;
        *newJoin = &tree->joinPool[tree->joinOffset];
        tree->joinOffset = tree->joinOffset + 1;        
    }

    return RULES_OK;
}

static unsigned int storeNode(ruleset *tree, 
                              node **newNode, 
                              unsigned int *nodeOffset) {

    if (!tree->nodePool) {
        tree->nodePool = malloc(sizeof(node));
        if (!tree->nodePool) {
            return ERR_OUT_OF_MEMORY;
        }

        *nodeOffset = 0;
        *newNode = &tree->nodePool[0];
        tree->nodeOffset = 1;
    } else {
        tree->nodePool = realloc(tree->nodePool, (tree->nodeOffset + 1) * sizeof(node));
        if (!tree->nodePool) {
            return ERR_OUT_OF_MEMORY;
        }

        *nodeOffset = tree->nodeOffset;
        *newNode = &tree->nodePool[tree->nodeOffset];
        tree->nodeOffset = tree->nodeOffset + 1;        
    }

    return RULES_OK;
}

static unsigned int storeAlpha(ruleset *tree, 
                               node **newNode, 
                               unsigned int *nodeOffset) {

    unsigned int result = storeNode(tree, newNode, nodeOffset);
    if (result != RULES_OK) {
        return result;
    }

    (*newNode)->value.a.nextListOffset = 0;
    (*newNode)->value.a.betaListOffset = 0;
    (*newNode)->value.a.nextOffset = 0;
    return RULES_OK;
}

static unsigned int allocateNext(ruleset *tree, 
                                 unsigned int length, 
                                 unsigned int *offset) {

    if (!tree->nextPool) {
        tree->nextPool = malloc((length + 1) * sizeof(unsigned int));
        if (!tree->nextPool) {
            return ERR_OUT_OF_MEMORY;
        }

        memset(tree->nextPool, 0, (length + 1) * sizeof(unsigned int));
        *offset = 1;
        tree->nextOffset = length + 1;
    } else {
        tree->nextPool = realloc(tree->nextPool, (tree->nextOffset + length) * sizeof(unsigned int));
        if (!tree->nextPool) {
            return ERR_OUT_OF_MEMORY;
        }

        memset(&tree->nextPool[tree->nextOffset], 0, length * sizeof(unsigned int));
        *offset = tree->nextOffset;
        tree->nextOffset = tree->nextOffset + length;
    }

    return RULES_OK;
}


static unsigned int ensureNextHashset(ruleset *tree, node *newNode) {
    if (!newNode->value.a.nextOffset) {
        return allocateNext(tree, NEXT_BUCKET_LENGTH, &newNode->value.a.nextOffset);
    }

    return RULES_OK;
}

static unsigned int ensureNextList(ruleset *tree, node *newNode) {
    if (!newNode->value.a.nextListOffset) {
        return allocateNext(tree, NEXT_LIST_LENGTH, &newNode->value.a.nextListOffset);
    }

    return RULES_OK;
}

static unsigned int ensureBetaList(ruleset *tree, node *newNode) {
    if (!newNode->value.a.betaListOffset) {
        return allocateNext(tree, BETA_LIST_LENGTH, &newNode->value.a.betaListOffset);
    }

    return RULES_OK;
}

static void copyValue(ruleset *tree, 
                      jsonValue *right, 
                      char *first, 
                      char *last,
                      unsigned int idiomOffset,
                      reference *ref,
                      unsigned char type) {
    right->type = type;
    unsigned int leftLength;
    char temp;
    switch(type) {
        case JSON_EVENT_PROPERTY:
        case JSON_STATE_PROPERTY:
            right->value.property.hash = ref->hash;
            right->value.property.nameOffset = ref->nameOffset;
            right->value.property.idOffset = ref->idOffset;
            break;
        case JSON_STATE_IDIOM:
        case JSON_EVENT_IDIOM:
            right->value.idiomOffset = idiomOffset;
            break;
        case JSON_STRING:
            leftLength = last - first;
            storeString(tree, first, &right->value.stringOffset, leftLength);
            break;
        case JSON_INT:
            temp = last[1];
            last[1] = '\0';
            right->value.i = atol(first);
            last[1] = temp;
            break;
        case JSON_DOUBLE:
            temp = last[1];
            last[1] = '\0';
            right->value.d = atof(first);
            last[1] = temp;
            break;
        case JSON_BOOL:
            leftLength = last - first + 1;
            unsigned char leftb = 1;
            if (leftLength == 5 && strncmp("false", first, 5)) {
                leftb = 0;
            }
            right->value.b = leftb;
            break;
    }
}

static unsigned char compareValue(ruleset *tree, 
                                  jsonValue *right, 
                                  char *first, 
                                  char *last,
                                  reference *ref,
                                  unsigned char type) {
    
    if (right->type != type) {
        return 0;
    }

    unsigned int leftLength;
    char temp;
    switch(type) {
        case JSON_EVENT_PROPERTY:
        case JSON_STATE_PROPERTY:
            if (right->value.property.hash == ref->hash &&
                right->value.property.nameOffset == ref->nameOffset &&
                right->value.property.idOffset == ref->idOffset)
                return 1;

            return 0;
        case JSON_STATE_IDIOM:
        case JSON_EVENT_IDIOM:
            return 0;
        case JSON_STRING:
            leftLength = last - first;
            char *rightString = &tree->stringPool[right->value.stringOffset];
            unsigned int rightLength = strlen(rightString);
            return (leftLength == rightLength ? !strncmp(rightString, first, rightLength): 0);
        case JSON_INT:
            temp = last[1];
            last[1] = '\0';
            long lefti = atol(first);
            last[1] = temp;
            return (right->value.i == lefti);
        case JSON_DOUBLE:
            temp = last[1];
            last[1] = '\0';
            double leftd = atof(first);
            last[1] = temp;
            return (right->value.d == leftd);
        case JSON_BOOL:
            leftLength = last - first + 1;
            unsigned char leftb = 1;
            if ((leftLength == 5 && strncmp("false", first, 5)) ||
                (leftLength == 1 && first[0] == '0')) {
                leftb = 0;
            } 
            return (right->value.b == leftb);     
    }

    return 0;
}

static unsigned int validateWindowSize(char *rule) {
    char *first;
    char *last;
    unsigned int hash;
    unsigned char type;
    unsigned int result = readNextName(rule, &first, &last, &hash);
    while (result == PARSE_OK) {
        if (hash == HASH_COUNT) {
            result = readNextValue(last, &first, &last, &type);
            if (type != JSON_INT) {
                return ERR_UNEXPECTED_TYPE;
            }

            return PARSE_OK;
        }

        result = readNextName(last, &first, &last, &hash);
    }

    return PARSE_OK;
}

static unsigned int validatePriority(char *rule) {
    char *first;
    char *last;
    unsigned int hash;
    unsigned char type;
    unsigned int result = readNextName(rule, &first, &last, &hash);
    while (result == PARSE_OK) {
        if (hash == HASH_PRI) {
            result = readNextValue(last, &first, &last, &type);
            if (type != JSON_INT) {
                return ERR_UNEXPECTED_TYPE;
            }

            return PARSE_OK;
        }

        result = readNextName(last, &first, &last, &hash);
    }

    return PARSE_OK;
}

static unsigned int validateReference(char *rule, unsigned char *referenceType) {
    char *first;
    char *last;
    unsigned char type;
    unsigned int hash;
    unsigned int result;

    result = readNextName(rule, &first, &last, &hash);
    if (result != PARSE_OK) {
        return result;
    }

    if (hash != HASH_S) {
        *referenceType = JSON_EVENT_PROPERTY;
        result = readNextString(last, &first, &last, &hash);
        if (result != PARSE_OK) {
            return result;
        }        
    } else {
        *referenceType = JSON_STATE_PROPERTY;
        if (readNextString(last, &first, &last, &hash) != PARSE_OK) {
            result = readNextValue(last, &first, &last, &type);
            if (result != PARSE_OK) {
                return result;
            }  
            
            unsigned int result = readNextName(first, &first, &last, &hash);
            while (result == PARSE_OK) {
                switch (hash) {
                    case HASH_NAME:
                        result = readNextString(last, &first, &last, &hash);
                        break;
                    case HASH_ID:
                        result = readNextValue(last, &first, &last, &type);
                        break;
                    default:
                        result = readNextValue(last, &first, &last, &type);
                        break;
                }

                if (result != PARSE_OK) {
                    return result;
                }  
                result = readNextName(last, &first, &last, &hash);
            }
        }
    }

    return PARSE_OK;
}

static unsigned int validateIdiom(char *rule, unsigned char *idiomType) {
    char *first;
    char *last;
    unsigned char type;
    unsigned int hash;
    unsigned int result;
    
    result = readNextName(rule, &first, &last, &hash);
    if (result != PARSE_OK) {
        return result;
    }

    if (hash != HASH_ADD && hash != HASH_SUB &&
        hash != HASH_MUL && hash != HASH_DIV) {
        return validateReference(rule, idiomType);
    } else {
        result = readNextValue(last, &first, &last, &type);
        if (result != PARSE_OK) {
            return result;
        }
        
        *idiomType = 0;
        result = readNextName(first, &first, &last, &hash);
        while (result == PARSE_OK) {
            unsigned char newIdiomType = 0;
            result = readNextValue(last, &first, &last, &type);
            if (result != PARSE_OK) {
                return result;
            }

            if (type == JSON_OBJECT) {
                result = validateIdiom(first, &newIdiomType);
                if (result != PARSE_OK) {
                    return result;
                }
            }

            if (newIdiomType == JSON_EVENT_PROPERTY || newIdiomType == JSON_EVENT_IDIOM) {
                if (*idiomType == JSON_STATE_PROPERTY || *idiomType == JSON_STATE_IDIOM) {
                    return ERR_UNEXPECTED_TYPE;
                }

                *idiomType = JSON_EVENT_IDIOM;
            } 

            if (newIdiomType == JSON_STATE_PROPERTY || newIdiomType == JSON_STATE_IDIOM) {
                if (*idiomType == JSON_EVENT_PROPERTY || *idiomType == JSON_EVENT_IDIOM) {
                    return ERR_UNEXPECTED_TYPE;
                }

                *idiomType = JSON_STATE_IDIOM;
            }                    

            if (hash != HASH_L && hash != HASH_R) {
                return ERR_UNEXPECTED_NAME;
            }

            result = readNextName(last, &first, &last, &hash);
        }
    }

    return PARSE_OK;
}


static unsigned int validateExpression(char *rule) {
    char *first;
    char *last;
    unsigned char type;
    unsigned char operator = OP_NOP;
    unsigned int hash;
    unsigned int result = readNextName(rule, &first, &last, &hash);
    if (result != PARSE_OK) {
        return result;
    }

    switch (hash) {
        case HASH_EQ:
            operator = OP_EQ;
            break;
        case HASH_NEQ:
            operator = OP_NEQ;
            break;
        case HASH_GT:
            operator = OP_GT;
            break;
        case HASH_GTE:
            operator = OP_GTE;
            break;
        case HASH_EX:
            operator = OP_EX;
            break;
        case HASH_NEX:
            operator = OP_NEX;
            break;
        case HASH_LT:
            operator = OP_LT;
            break;
        case HASH_LTE:
            operator = OP_LTE;
            break;
        case HASH_AND:
        case HASH_OR:
            result = readNextValue(last, &first, &last, &type);
            if (type != JSON_ARRAY) {
                return ERR_UNEXPECTED_TYPE;
            }

            result = readNextArrayValue(first, &first, &last, &type);
            while (result == PARSE_OK) {
                result = validateExpression(first);
                if (result != PARSE_OK) {
                    return result;
                }

                result = readNextArrayValue(last, &first, &last, &type);   
            }

            return (result == PARSE_END ? RULES_OK: result);
    }

    if (operator == OP_NOP) {
        operator = OP_EQ;
        first = rule;
    } else {
        result = readNextValue(last, &first, &last, &type);
        if (result != PARSE_OK) {
            return result;
        }
        if (type != JSON_OBJECT) {
            return ERR_UNEXPECTED_TYPE;
        }
    }
    
    result = readNextName(first, &first, &last, &hash);
    if (result != PARSE_OK) {
        return result;
    }

    // Validating expression rValue
    result = readNextValue(last, &first, &last, &type);
    if (result != PARSE_OK) {
        return result;
    }

    if (type == JSON_ARRAY) {
        return ERR_UNEXPECTED_TYPE;
    }

    if (type == JSON_OBJECT) {
        unsigned char idiomType = 0;
        result = validateIdiom(first, &idiomType);
        if (result != PARSE_OK) {
            return result;
        }
    }

    return PARSE_OK;
}

static unsigned int validateAlgebra(char *rule) {
    char *first;
    char *last;
    char *lastArrayValue;
    unsigned int hash;
    unsigned char type;
    unsigned char reenter = 0;
    unsigned int result = readNextArrayValue(rule, &first, &lastArrayValue, &type);
    while (result == PARSE_OK) {
        result = validateWindowSize(first);
        if (result != PARSE_OK) {
            return result;
        }

        result = readNextName(first, &first, &last, &hash);
        // Fast foward $count
        if (hash == HASH_COUNT) {
            readNextValue(last, &first, &last, &type);
            result = readNextName(last, &first, &last, &hash);
            if (result != PARSE_OK) {
                return (result == PARSE_END ? RULES_OK: result);
            }
        }

        unsigned int nameLength = last - first; 
        if (nameLength >= 4) {
            if (!strncmp("$all", last - 4, 4)) {
                nameLength = nameLength - 4;
                reenter = 1;
            } else if (!strncmp("$any", last - 4, 4)) {
                nameLength = nameLength - 4;
                reenter = 1;
            } 
        } 
        
        result = readNextValue(last, &first, &last, &type);
        if (result != PARSE_OK) {
            return result;
        }
        
        if (!reenter) {
            result = validateExpression(first);
        }
        else {
            result = validateAlgebra(first);
            reenter = 0;
        }

        if (result != PARSE_OK) {
            return result;
        }

        result = readNextArrayValue(lastArrayValue, &first, &lastArrayValue, &type);
    }

    return (result == PARSE_END ? PARSE_OK: result);
}

static unsigned int validateRuleset(char *rules) {
    char *first;
    char *last;
    char *firstName;
    char *lastName;
    char *lastRuleValue;
    unsigned char type;
    unsigned int hash;
    unsigned int result;
    result = readNextName(rules, &firstName, &lastName, &hash);
    while (result == PARSE_OK) {
        result = readNextValue(lastName, &first, &lastRuleValue, &type);
        if (result != PARSE_OK) {
            return result;
        }
        if (type != JSON_OBJECT) {
            return ERR_UNEXPECTED_TYPE;
        }

        result = validateWindowSize(first);
        if (result != PARSE_OK) {
            return result;
        }

        result = validatePriority(first);
        if (result != PARSE_OK) {
            return result;
        }

        result = readNextName(first, &first, &last, &hash);
        while (result == PARSE_OK) {
            result = readNextValue(last, &first, &last, &type);
            if (result != PARSE_OK) {
                return result;
            }
            
            if (hash == HASH_ALL || hash == HASH_ANY) {
                result = validateAlgebra(first);
                if (result != RULES_OK && result != PARSE_END) {
                    return result;
                }
            } else if (hash != HASH_COUNT && hash != HASH_PRI) {
                return ERR_UNEXPECTED_NAME;
            }

            result = readNextName(last, &first, &last, &hash);
        }

        result = readNextName(lastRuleValue, &firstName, &lastName, &hash);
    }

    return (result == PARSE_END ? PARSE_OK: result);
}

static unsigned int linkAlpha(ruleset *tree, 
                              unsigned int parentOffset, 
                              unsigned int nextOffset) {
    unsigned int result;
    unsigned int entry;
    node *parentAlpha = &tree->nodePool[parentOffset];
    node *nextNode = &tree->nodePool[nextOffset];
    if (nextNode->type != NODE_ALPHA) {
        result = ensureBetaList(tree, parentAlpha);
        if (result != RULES_OK) {
            return result;
        }

        unsigned int *parentBetaList = &tree->nextPool[parentAlpha->value.a.betaListOffset];
        for (entry = 0; parentBetaList[entry] != 0; ++entry) {
            if (entry == BETA_LIST_LENGTH) {
                return ERR_RULE_BETA_LIMIT_EXCEEDED;
            }
        }

        parentBetaList[entry] = nextOffset;
    }
    else if (nextNode->value.a.operator == OP_NEX) {
        result = ensureNextList(tree, parentAlpha);
        if (result != RULES_OK) {
            return result;
        }

        unsigned int *parentNextList = &tree->nextPool[parentAlpha->value.a.nextListOffset];
        unsigned int entry;
        for (entry = 0; parentNextList[entry] != 0; ++entry) {
            if (entry == NEXT_LIST_LENGTH) {
                return ERR_RULE_EXISTS_LIMIT_EXCEEDED;
            }
        }

        parentNextList[entry] = nextOffset;
    }
    else {
        result = ensureNextHashset(tree, parentAlpha);
        if (result != RULES_OK) {
            return result;
        }

        unsigned int *parentNext = &tree->nextPool[parentAlpha->value.a.nextOffset];
        unsigned int hash = nextNode->value.a.hash;
        for (entry = hash & HASH_MASK; parentNext[entry] != 0; entry = (entry + 1) % NEXT_BUCKET_LENGTH) {
            if ((entry + 1) % NEXT_BUCKET_LENGTH == (hash & HASH_MASK)) {
                return ERR_RULE_LIMIT_EXCEEDED;
            }
        }

        parentNext[entry] = nextOffset;
    }

    return RULES_OK;
}

static unsigned char readReference(ruleset *tree, char *rule, reference *ref) {
    char *first;
    char *last;
    unsigned char type;
    unsigned int hash;
    unsigned char returnType = JSON_EVENT_PROPERTY;
    
    ref->idOffset = 0;
    readNextName(rule, &first, &last, &hash);
    if (hash != HASH_S) {
        storeString(tree, first, &ref->idOffset, last - first);   
        readNextString(last, &first, &last, &hash);
        ref->hash = hash;
        storeString(tree, first, &ref->nameOffset, last - first);
    } else {
        returnType = JSON_STATE_PROPERTY;
        if (readNextString(last, &first, &last, &hash) == PARSE_OK) {
            ref->hash = hash;
            storeString(tree, first, &ref->nameOffset, last - first);
        }
        else {
            readNextValue(last, &first, &last, &type);
            unsigned int result = readNextName(first, &first, &last, &hash);
            while (result == PARSE_OK) {
                switch (hash) {
                    case HASH_NAME:
                        readNextString(last, &first, &last, &hash);
                        ref->hash = hash;
                        storeString(tree, first, &ref->nameOffset, last - first);
                        break;
                    case HASH_ID:
                        readNextValue(last, &first, &last, &type);
                        if (type == JSON_STRING) {
                            storeString(tree, first, &ref->idOffset, last - first);
                        } else{
                            storeString(tree, first, &ref->idOffset, last - first + 1);
                        }

                        break;
                    default:
                        readNextValue(last, &first, &last, &type);
                        break;
                }

                result = readNextName(last, &first, &last, &hash);
            }
        }
    }

    return returnType;
}

static unsigned int readIdiom(ruleset *tree, char *rule, unsigned char *idiomType, unsigned int *idiomOffset, reference *ref) {
    char *first;
    char *last;
    unsigned char type;
    unsigned char operator = OP_NOP;
    unsigned int hash;
    unsigned int result;
    
    readNextName(rule, &first, &last, &hash);
    switch (hash) {
        case HASH_ADD:
            operator = OP_ADD;
            break;
        case HASH_SUB:
            operator = OP_SUB;
            break;
        case HASH_MUL:
            operator = OP_MUL;
            break;
        case HASH_DIV:
            operator = OP_DIV;
            break;
    }

    if (operator == OP_NOP) {
        *idiomType = readReference(tree, rule, ref);
    } else {
        idiom *newIdiom = NULL;
        result = storeIdiom(tree, &newIdiom, idiomOffset);
        if (result != RULES_OK) {
            return result;
        }

        *idiomType = JSON_STATE_IDIOM;  
        newIdiom->operator = operator;
        readNextValue(last, &first, &last, &type);
        result = readNextName(first, &first, &last, &hash);
        while (result == PARSE_OK) {
            unsigned int newIdiomOffset = 0;
            reference newRef;
            readNextValue(last, &first, &last, &type);
            if (type == JSON_OBJECT) {
                result = readIdiom(tree, first, &type, &newIdiomOffset, &newRef);
                if (result != RULES_OK) {
                    return result;
                }
            }

            if (type == JSON_EVENT_PROPERTY || type == JSON_EVENT_IDIOM) {
                *idiomType = JSON_EVENT_IDIOM;  
            }

            switch (hash) {
                case HASH_L:
                    copyValue(tree, &newIdiom->left, first, last, newIdiomOffset, &newRef, type);
                    break;
                case HASH_R:
                    copyValue(tree, &newIdiom->right, first, last, newIdiomOffset, &newRef, type);
                    break;
            }

            result = readNextName(last, &first, &last, &hash);
        }
    }

    return RULES_OK;
}

static unsigned int findAlpha(ruleset *tree, 
                              unsigned int parentOffset, 
                              unsigned char operator, 
                              char *rule,
                              expression *expr, 
                              unsigned int *resultOffset) {
    char *first;
    char *last;
    char *firstName;
    char *lastName;
    unsigned char type;
    unsigned int entry;
    unsigned int hash;
    unsigned int idiomOffset;
    unsigned int result;
    reference ref;
    
    readNextName(rule, &firstName, &lastName, &hash);
    readNextValue(lastName, &first, &last, &type);
    if (type == JSON_OBJECT) {
        result = readIdiom(tree, first, &type, &idiomOffset, &ref);
        if (result != RULES_OK) {
            return result;
        }
    }

    node *parent = &tree->nodePool[parentOffset];
    unsigned int *parentNext;
    if (parent->value.a.nextOffset) {
        parentNext = &tree->nextPool[parent->value.a.nextOffset];
        for (entry = hash & HASH_MASK; parentNext[entry] != 0; entry = (entry + 1) % NEXT_BUCKET_LENGTH) {
            node *currentNode = &tree->nodePool[parentNext[entry]];
            if (currentNode->value.a.hash == hash && 
                currentNode->value.a.operator == operator) {
                if (compareValue(tree, &currentNode->value.a.right, first, last, &ref, type)) {
                    *resultOffset = parentNext[entry];
                    return RULES_OK;
                }
            }
        }
    }
    
    if (parent->value.a.nextListOffset) {
        parentNext = &tree->nextPool[parent->value.a.nextListOffset];
        for (entry = 0; parentNext[entry] != 0; ++entry) {
            node *currentNode = &tree->nodePool[parentNext[entry]];
            if (currentNode->value.a.hash == hash&& 
                currentNode->value.a.operator == operator) {
                if (compareValue(tree, &currentNode->value.a.right, first, last, &ref, type)) {
                    *resultOffset = parentNext[entry];
                    return RULES_OK;
                }
            }
        }
    }

    unsigned int stringOffset;
    result = storeString(tree, firstName, &stringOffset, lastName - firstName);
    if (result != RULES_OK) {
        return result;
    }

    node *newAlpha;
    result = storeAlpha(tree, &newAlpha, resultOffset);
    if (result != RULES_OK) {
        return result;
    }
    
    newAlpha->nameOffset = stringOffset;
    newAlpha->type = NODE_ALPHA;
    newAlpha->value.a.hash = hash;
    newAlpha->value.a.operator = operator;
    copyValue(tree, &newAlpha->value.a.right, first, last, idiomOffset, &ref, type);
    if (type == JSON_EVENT_PROPERTY || type == JSON_EVENT_IDIOM) {
        result = appendTerm(expr, *resultOffset);
        if (result != RULES_OK) {
            return result;
        }
    }

    return linkAlpha(tree, parentOffset, *resultOffset);
}

static void getWindowSize(char *rule, unsigned short *count) {
    char *first;
    char *last;
    char temp;
    unsigned int hash;
    unsigned char type;
    unsigned int result = readNextName(rule, &first, &last, &hash);
    while (result == PARSE_OK) {
        readNextValue(last, &first, &last, &type);
        if (hash == HASH_COUNT) {
            temp = first[last - first + 1];
            first[last - first + 1] = '\0';
            *count = atoi(first);
            first[last - first + 1] = temp;
            break;   
        }
        result = readNextName(last, &first, &last, &hash);
    }
}

static void getPriority(char *rule, unsigned short *priority) {
    char *first;
    char *last;
    char temp;
    unsigned int hash;
    unsigned char type;
    unsigned int result = readNextName(rule, &first, &last, &hash);
    while (result == PARSE_OK) {
        readNextValue(last, &first, &last, &type);
        if (hash == HASH_PRI) {
            temp = first[last - first + 1];
            first[last - first + 1] = '\0';
            *priority = atoi(first);
            first[last - first + 1] = temp;
            break;   
        }
        result = readNextName(last, &first, &last, &hash);
    }
}

static unsigned int createAlpha(ruleset *tree, 
                                char *rule, 
                                expression *expr,
                                unsigned int nextOffset,
                                unsigned int *newOffset) {
    char *first;
    char *last;
    unsigned char type;
    unsigned char operator = OP_NOP;
    unsigned int hash;
    unsigned int result;
    unsigned int parentOffset = *newOffset;
    readNextName(rule, &first, &last, &hash);
    switch (hash) {
        case HASH_EQ:
            operator = OP_EQ;
            break;
        case HASH_NEQ:
            operator = OP_NEQ;
            break;
        case HASH_GT:
            operator = OP_GT;
            break;
        case HASH_GTE:
            operator = OP_GTE;
            break;
        case HASH_EX:
            operator = OP_EX;
            break;
        case HASH_NEX:
            operator = OP_NEX;
            break;
        case HASH_LT:
            operator = OP_LT;
            break;
        case HASH_LTE:
            operator = OP_LTE;
            break;
        case HASH_AND:
            result = appendTerm(expr, tree->andNodeOffset);
            if (result != RULES_OK) {
                return result;
            }

            readNextValue(last, &first, &last, &type);
            unsigned int previousOffset = 0;
            unsigned int resultOffset = parentOffset;
            result = readNextArrayValue(first, &first, &last, &type);
            while (result == PARSE_OK) {
                createAlpha(tree, first, expr, 0, &resultOffset);
                previousOffset = resultOffset;
                result = readNextArrayValue(last, &first, &last, &type);   
            }

            result = appendTerm(expr, tree->endNodeOffset);
            if (result != RULES_OK) {
                return result;
            }
            return linkAlpha(tree, previousOffset, nextOffset);
        case HASH_OR:
            result = appendTerm(expr, tree->orNodeOffset);
            if (result != RULES_OK) {
                return result;
            }

            readNextValue(last, &first, &last, &type);
            result = readNextArrayValue(first, &first, &last, &type);
            while (result == PARSE_OK) {
                unsigned int single_offset = parentOffset;
                createAlpha(tree, first, expr, nextOffset, &single_offset);
                result = readNextArrayValue(last, &first, &last, &type);   
            }

            result = appendTerm(expr, tree->endNodeOffset);
            if (result != RULES_OK) {
                return result;
            }
            return RULES_OK;
    }

    if (operator == OP_NOP) {
        operator = OP_EQ;
        first = rule;
    } else {
        readNextValue(last, &first, &last, &type);
    }
    
    if (nextOffset == 0) {
        return findAlpha(tree, parentOffset, operator, first, expr, newOffset);
    } else {
        unsigned int result = findAlpha(tree, parentOffset, operator, first, expr, newOffset);
        if (result != RULES_OK) {
            return result;
        }

        return linkAlpha(tree, *newOffset, nextOffset);
    }
}

static unsigned int createBetaConnector(ruleset *tree, 
                                        char *rule, 
                                        path *betaPath,
                                        unsigned int nextOffset) {
    char *first;
    char *last;
    char *lastArrayValue;
    unsigned int hash;
    unsigned char type; 
    unsigned int result = readNextArrayValue(rule, &first, &lastArrayValue, &type);
    while (result == PARSE_OK) {
        unsigned short count = 1;
        getWindowSize(first, &count);
        readNextName(first, &first, &last, &hash);
        
        // Fast foward $count
        if (hash == HASH_COUNT) {
            readNextValue(last, &first, &last, &type);
            result = readNextName(last, &first, &last, &hash);
            if (result != PARSE_OK) {
                return (result == PARSE_END ? RULES_OK: result);
            }
        }

        unsigned int nameLength = last - first; 
        unsigned char operator = OP_NOP;
        if (nameLength >= 4) { 
            if (!strncmp("$all", last - 4, 4)) {
                nameLength = nameLength - 4;
                operator = OP_ALL;
            } else if (!strncmp("$any", last - 4, 4)) {
                nameLength = nameLength - 4;
                operator = OP_ANY;
            }
        }        
        
        unsigned int stringOffset;
        result = storeString(tree, first, &stringOffset, nameLength);
        if (result != RULES_OK) {
            return result;
        }

        node *connector;
        unsigned int connectorOffset;
        result = storeNode(tree, &connector, &connectorOffset);
        if (result != RULES_OK) {
            return result;
        }

        expression *expr;
        connector->nameOffset = stringOffset;
        connector->type = NODE_BETA_CONNECTOR;
        connector->value.b.nextOffset = nextOffset;
        if (betaPath->expressionsLength == 0) {
            betaPath->expressionsLength = 1;
            betaPath->expressions = malloc(sizeof(expression));
            if (!betaPath->expressions) {
                return ERR_OUT_OF_MEMORY;
            }

            expr = betaPath->expressions;
        }
        else {
            betaPath->expressionsLength = betaPath->expressionsLength + 1;
            betaPath->expressions = realloc(betaPath->expressions, betaPath->expressionsLength * sizeof(expression));
            if (!betaPath->expressions) {
                return ERR_OUT_OF_MEMORY;
            }

            expr = &betaPath->expressions[betaPath->expressionsLength - 1];
        }

        expr->nameOffset = stringOffset;
        expr->aliasOffset = stringOffset;
        expr->termsLength = 0;
        expr->t.termsPointer = NULL;
        if (operator == OP_NOP) {
            unsigned int resultOffset = NODE_M_OFFSET;
            readNextValue(last, &first, &last, &type);
            result = createAlpha(tree, first, expr, connectorOffset, &resultOffset);
        }
        else {
            readNextValue(last, &first, &last, &type);
            result = createBeta(tree, first, operator, connectorOffset, betaPath, count, NULL);
        }

        if (result != RULES_OK) {
            return result;
        }

        result = readNextArrayValue(lastArrayValue, &first, &lastArrayValue, &type);
    }

    return (result == PARSE_END ? RULES_OK: result);
}

static unsigned int createBeta(ruleset *tree, 
                               char *rule, 
                               unsigned char operator, 
                               unsigned int nextOffset, 
                               path *nextPath, 
                               unsigned short count,
                               path **outPath) {

    path *betaPath = malloc(sizeof(path));
    if (!betaPath) {
        return ERR_OUT_OF_MEMORY;
    }

    betaPath->operator = operator;
    betaPath->expressionsLength = 0;
    betaPath->expressions = NULL;
    betaPath->parents = NULL;
    betaPath->parentsLength = 0;
    betaPath->count = count;

    if (nextPath) {
        if (nextPath->parentsLength == 0) {
            nextPath->parentsLength = nextPath->expressionsLength;
            nextPath->parents = calloc(nextPath->expressionsLength, sizeof(path*));
            if (!nextPath->parents) {
                return ERR_OUT_OF_MEMORY;
            }

            nextPath->parents[nextPath->expressionsLength - 1] = betaPath;
        } else {
            int lengthDiff = nextPath->expressionsLength - nextPath->parentsLength;
            nextPath->parents = realloc(nextPath->parents, sizeof(path*) * nextPath->expressionsLength);
            if (!nextPath->parents) {
                return ERR_OUT_OF_MEMORY;
            }

            for (int i = 0; i < lengthDiff -1; ++i) {
                nextPath->parents[nextPath->parentsLength + i] = NULL;
            }

            nextPath->parentsLength = nextPath->expressionsLength;
            nextPath->parents[nextPath->parentsLength - 1] = betaPath;
        }
    }

    if (outPath) {
        *outPath = betaPath;
    }

    return createBetaConnector(tree, rule, betaPath, nextOffset);    
}

static unsigned int add(any *right, any *left, any **result) {
    right->nodes = realloc(right->nodes, (right->nodesLength + left->nodesLength) * sizeof(all*));
    if (!right->nodes) {
        return ERR_OUT_OF_MEMORY;
    }
    for (unsigned int i = 0; i < left->nodesLength; ++i) {
        right->nodes[right->nodesLength + i] = left->nodes[i];
    }
    right->nodesLength = right->nodesLength + left->nodesLength;
    free(left->nodes);
    free(left);
    *result = right;
    return RULES_OK;
}

static unsigned int multiply(ruleset *tree, any *right, any *left, any **result) {
    any *product = malloc(sizeof(any));
    product->nodesLength = right->nodesLength * left->nodesLength;
    product->nodes = malloc(sizeof(all*) * product->nodesLength);

    for (unsigned int i = 0; i < left->nodesLength; ++i) {
        all *leftAll = left->nodes[i];
        for (unsigned int ii = 0; ii < right->nodesLength; ++ii) {
            all *rightAll = right->nodes[ii];
            all *newAll = malloc(sizeof(all));
            if (!newAll) {
                return ERR_OUT_OF_MEMORY;
            }

            newAll->count = (leftAll->count > rightAll->count ? leftAll->count: rightAll->count);
            newAll->expressions = malloc((rightAll->expressionsLength + leftAll->expressionsLength) * sizeof(unsigned int));
            if (!newAll->expressions) {
                return ERR_OUT_OF_MEMORY;
            }

            for (unsigned int iii = 0; iii < rightAll->expressionsLength; ++iii) {
                newAll->expressions[iii] =  rightAll->expressions[iii];
            }

            for (unsigned int iii = 0; iii < leftAll->expressionsLength; ++iii) {
                newAll->expressions[rightAll->expressionsLength + iii] = leftAll->expressions[iii];
            }

            newAll->expressionsLength = rightAll->expressionsLength + leftAll->expressionsLength;
            product->nodes[i + ii * left->nodesLength] = newAll;
        }

        free(leftAll->expressions);
        free(leftAll);
    }

    for (unsigned int i = 0; i < right->nodesLength; ++i) {
        all *rightAll = right->nodes[i];
        free(rightAll->expressions);
        free(rightAll);
    }

    free(right->nodes);
    free(right);
    free(left->nodes);
    free(left);
    *result = product;
    return RULES_OK;
}

static unsigned int createSingleQuery(ruleset *tree, 
                                      char *name, 
                                      expression *expr, 
                                      unsigned short count, 
                                      any **resultAny) {
    any *newAny = malloc(sizeof(any));
    if (!newAny) {
        return ERR_OUT_OF_MEMORY;
    }

    newAny->nodesLength = 1;
    newAny->nodes = malloc(sizeof(all*));
    if (!newAny->nodes) {
        return ERR_OUT_OF_MEMORY;
    }

    newAny->nodes[0] = malloc(sizeof(all));
    if (!newAny->nodes[0]) {
        return ERR_OUT_OF_MEMORY;
    }

    newAny->nodes[0]->count = count;
    newAny->nodes[0]->expressionsLength = 1;
    newAny->nodes[0]->expressions = malloc(sizeof(unsigned int));
    if (!newAny->nodes[0]->expressions) {
        return ERR_OUT_OF_MEMORY;
    }

    expression *newExpression;
    unsigned int result = storeExpression(tree, &newExpression, newAny->nodes[0]->expressions);
    if (result != RULES_OK) {
        return result;
    }

    result = storeString(tree, name, &newExpression->nameOffset, strlen(name));
    if (result != RULES_OK) {
        return result;
    }

    newExpression->aliasOffset = expr->aliasOffset;
    newExpression->termsLength = expr->termsLength;
    if (expr->termsLength) {
        result = allocateNext(tree, expr->termsLength, &newExpression->t.termsOffset);
        if (result != RULES_OK) {
            return result;
        }

        for (unsigned short i = 0; i < expr->termsLength; ++i) {
            tree->nextPool[newExpression->t.termsOffset + i] = expr->t.termsPointer[i];
        }

        free(expr->t.termsPointer);
    }

    *resultAny = newAny;
    return RULES_OK;
}

static unsigned int createQueries(ruleset *tree, 
                                  char *postfix, 
                                  path *betaPath, 
                                  any **anyResult) {
    any *currentAny = NULL;
    for (unsigned short i = 0; i < betaPath->expressionsLength; ++ i) {
        unsigned int result;
        expression *expr = &betaPath->expressions[i];
        if (betaPath->operator == OP_NOP) {
            result = createSingleQuery(tree, postfix, expr, betaPath->count, anyResult);
            if (result != RULES_OK) {
                return result;
            }

            free(betaPath->expressions);
            free(betaPath);
            return RULES_OK;
        }

        char *name = &tree->stringPool[expr->nameOffset];
        int nameLength = strlen(name);
        char nextPostfix[strlen(postfix) + nameLength + 2];
        strcpy(nextPostfix, name);
        nextPostfix[nameLength] = '!';
        strcpy(&nextPostfix[nameLength + 1], postfix);
        
        any *newAny = NULL;    
        if (i >= betaPath->parentsLength || !betaPath->parents[i]) {
            result = createSingleQuery(tree, nextPostfix, expr, betaPath->count, &newAny);
            if (result != RULES_OK) {
                return result;
            }
        }
        else {
            result = createQueries(tree, nextPostfix, betaPath->parents[i], &newAny);
            if (result != RULES_OK) {
                return result;
            }
        }

        if (!currentAny) {
            currentAny = newAny;
        }
        else if (betaPath->operator == OP_ANY) {
            result = add(currentAny, newAny, &currentAny);
            if (result != RULES_OK) {
                return result;
            }
        }
        else if (betaPath->operator == OP_ALL) {
            result = multiply(tree, currentAny, newAny, &currentAny);
            if (result != RULES_OK) {
                return result;
            }
        }
    }

    free(betaPath->expressions);
    free(betaPath->parents);
    free(betaPath);
    *anyResult = currentAny;
    return RULES_OK;
}

static unsigned int fixupQueries(ruleset *tree, unsigned int actionOffset, char* postfix, path *betaPath) {
    node *actionNode = &tree->nodePool[actionOffset];
    char copyPostfix[strlen(postfix) + 1];
    strcpy(copyPostfix, postfix);
    
    any *query;
    unsigned int result = createQueries(tree, copyPostfix, betaPath, &query);
    if (result != RULES_OK) {
        return result;
    }

    actionNode->value.c.joinsLength = query->nodesLength;
    result = allocateNext(tree, query->nodesLength, &actionNode->value.c.joinsOffset);
    if (result != RULES_OK) {
        return result;
    }

    for (unsigned int i = 0; i < query->nodesLength; ++i) {
        all *currentAll = query->nodes[i];

        join *newJoin;
        result = storeJoin(tree, &newJoin, &tree->nextPool[actionNode->value.c.joinsOffset + i]);
        if (result != RULES_OK) {
            return result;
        }
        
        newJoin->count = currentAll->count;
        newJoin->expressionsLength = currentAll->expressionsLength;
        result = allocateNext(tree, currentAll->expressionsLength, &newJoin->expressionsOffset);
        if (result != RULES_OK) {
            return result;
        }

        for (unsigned int ii = 0; ii < currentAll->expressionsLength; ++ii) {
            tree->nextPool[newJoin->expressionsOffset + ii] = currentAll->expressions[ii];
        }

        free(currentAll->expressions);
        free(currentAll);
    }
    
    free(query->nodes);
    free(query);
    return RULES_OK;
}

static unsigned int createTree(ruleset *tree, char *rules) {
    char *first;
    char *last;
    char *lastRuleValue;
    char *firstName;
    char *lastName;
    unsigned char type;
    unsigned int hash;
    unsigned int result = readNextName(rules, &firstName, &lastName, &hash);
    while (result == PARSE_OK) {
        path *betaPath = NULL;
        node *ruleAction;
        unsigned int actionOffset;
        result = storeNode(tree, &ruleAction, &actionOffset);
        if (result != RULES_OK) {
            return result;
        }
        
        ruleAction->value.c.index = tree->actionCount;
        ++tree->actionCount;
        ruleAction->type = NODE_ACTION;

        // tree->stringPool can change after storing strings
        // need to resolve namespace every time it is used.
        char *namespace = &tree->stringPool[tree->nameOffset];
        int namespaceLength = strlen(namespace); 
        char runtimeActionName[namespaceLength + lastName - firstName + 2];
        strncpy(runtimeActionName, firstName, lastName - firstName);
        runtimeActionName[lastName - firstName] = '!';
        strncpy(&runtimeActionName[lastName - firstName + 1], namespace, namespaceLength);
        runtimeActionName[namespaceLength + lastName - firstName + 1] = '\0';
        result = storeString(tree, runtimeActionName, &ruleAction->nameOffset, namespaceLength + lastName - firstName + 1);
        if (result != RULES_OK) {
            return result;
        }

        readNextValue(lastName, &first, &lastRuleValue, &type);
        getPriority(first, &ruleAction->value.c.priority);
        
        unsigned short count = 1;
        getWindowSize(first, &count);
        result = readNextName(first, &first, &last, &hash);
        while (result == PARSE_OK) {
            readNextValue(last, &first, &last, &type);
            switch (hash) {
                case HASH_ANY:
                    result = createBeta(tree, first, OP_ANY, actionOffset, NULL, count, &betaPath);
                    break;
                case HASH_ALL:
                    result = createBeta(tree, first, OP_ALL, actionOffset, NULL, count, &betaPath);
                    break;
            }
            result = readNextName(last, &first, &last, &hash);
        }

        result = fixupQueries(tree, actionOffset, runtimeActionName, betaPath);
        if (result != RULES_OK) {
            return result;
        }

        result = readNextName(lastRuleValue, &firstName, &lastName, &hash);
    }

    return RULES_OK;
}

unsigned int createRuleset(void **handle, char *name, char *rules, unsigned int stateCaheSize) {
    node *newNode;
    unsigned int stringOffset;
    unsigned int nodeOffset;
    unsigned int result = validateRuleset(rules);
    if (result != PARSE_OK) {
        return result;
    }

    ruleset *tree = malloc(sizeof(ruleset));
    if (!tree) {
        return ERR_OUT_OF_MEMORY;
    }
    
    tree->stringPool = NULL;
    tree->stringPoolLength = 0;
    tree->nodePool = NULL;
    tree->nodeOffset = 0;
    tree->nextPool = NULL;
    tree->nextOffset = 0;
    tree->expressionPool = NULL;
    tree->expressionOffset = 0;
    tree->idiomPool = NULL;
    tree->idiomOffset = 0;
    tree->joinPool = NULL;
    tree->joinOffset = 0;
    tree->actionCount = 0;
    tree->bindingsList = NULL;
    tree->stateLength = 0;
    tree->state = calloc(stateCaheSize, sizeof(stateEntry));
    tree->maxStateLength = stateCaheSize;
    tree->stateBucketsLength = stateCaheSize / 4;
    tree->stateBuckets = malloc(tree->stateBucketsLength * sizeof(unsigned int));
    memset(tree->stateBuckets, 0xFF, tree->stateBucketsLength * sizeof(unsigned int));
    tree->lruStateOffset = UNDEFINED_HASH_OFFSET;
    tree->mruStateOffset = UNDEFINED_HASH_OFFSET;

    result = storeString(tree, name, &tree->nameOffset, strlen(name));
    if (result != RULES_OK) {
        return result;
    }

    result = storeString(tree, "m", &stringOffset, 1);
    if (result != RULES_OK) {
        return result;
    }

    result = storeAlpha(tree, &newNode, &nodeOffset);
    if (result != RULES_OK) {
        return result;
    }

    newNode->nameOffset = stringOffset;
    newNode->type = NODE_ALPHA;
    newNode->value.a.operator = OP_TYPE;
    result = storeAlpha(tree, &newNode, &tree->andNodeOffset);
    if (result != RULES_OK) {
        return result;
    }

    newNode->nameOffset = 0;
    newNode->type = NODE_ALPHA;
    newNode->value.a.operator = OP_AND;
    result = storeAlpha(tree, &newNode, &tree->orNodeOffset);
    if (result != RULES_OK) {
        return result;
    }

    newNode->nameOffset = 0;
    newNode->type = NODE_ALPHA;
    newNode->value.a.operator = OP_OR;
    result = storeAlpha(tree, &newNode, &tree->endNodeOffset);
    if (result != RULES_OK) {
        return result;
    }

    newNode->nameOffset = 0;
    newNode->type = NODE_ALPHA;
    newNode->value.a.operator = OP_END;

    *handle = tree;

    // will use random numbers for state stored event mids
    srand(time(NULL));
    return createTree(tree, rules);
}

unsigned int deleteRuleset(void *handle) {
    ruleset *tree = (ruleset*)(handle);
    deleteBindingsList(tree);
    free(tree->nodePool);
    free(tree->nextPool);
    free(tree->stringPool);
    free(tree->expressionPool);
    free(tree->idiomPool);
    free(tree->joinPool);
    free(tree->stateBuckets);
    for (unsigned int i = 0; i < tree->stateLength; ++i) {
        stateEntry *entry = &tree->state[i];
        if (entry->state) {
            free(entry->state);
        }

        if (entry->sid) {
            free(entry->sid);
        }
    }
    free(tree->state);
    free(tree);
    return RULES_OK;
}



