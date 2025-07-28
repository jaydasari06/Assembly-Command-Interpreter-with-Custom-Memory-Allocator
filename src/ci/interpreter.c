#include "interpreter.h"
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#include "command_type.h"
#include "mem.h"

static bool    cond_holds(Interpreter *intr, BranchCondition cond);
static int64_t fetch_number_value(Interpreter *intr, Operand *op, bool is_im);
static bool    print_base(Interpreter *intr, Command *cmd);

void interpreter_init(Interpreter *intr, LabelMap *map) {
    if (!intr) {
        return;
    }

    intr->had_error  = false;
    intr->label_map  = map;
    intr->is_greater = false;
    intr->is_equal   = false;
    intr->is_less    = false;
    intr->the_stack  = NULL;

    for (size_t i = 0; i < NUM_VARIABLES; i++) {
        intr->variables[i] = 0;
    }
}

void interpret(Interpreter *intr, Command *commands) {
    if (!intr || !commands) {
        return;
    }
    Command *current = commands;
    while (current && !intr->had_error) {
        switch (current->type) {
            // STUDENT TODO: process the commands and take actions as appropriate
            case CMD_MOV:
                intr -> variables[current -> destination.num_val] = fetch_number_value(intr, &current -> val_a, true);
                current = current->next;
                break;
            case CMD_ADD:
                intr -> variables[current -> destination.num_val] = fetch_number_value(intr, &current -> val_a, false) 
                    + fetch_number_value(intr, &current -> val_b, current -> is_b_immediate);
                current = current->next;
                break;
            case CMD_SUB: {
                intr -> variables[current -> destination.num_val] = fetch_number_value(intr, &current -> val_a, false) 
                    - fetch_number_value(intr, &current -> val_b, current -> is_b_immediate);
                current = current->next;
                break;
            }
            case CMD_CMP:
                intr -> is_greater = false;
                intr -> is_equal = false;
                intr -> is_less = false;
                if (fetch_number_value(intr, &current -> val_a, false) > fetch_number_value(intr, &current -> val_b, current -> is_b_immediate)) {
                    intr -> is_greater = true;
                } else if (fetch_number_value(intr, &current -> val_a, false) < fetch_number_value(intr, &current -> val_b, current -> is_b_immediate)) {
                    intr -> is_less = true;
                } else {
                    intr -> is_equal = true;
                }
                current = current->next;
                break;
            case CMD_CMP_U:
                intr -> is_greater = false;
                intr -> is_equal   = false;
                intr -> is_less    = false;
                if ((uint64_t) fetch_number_value(intr, &current -> val_a, false) > (uint64_t) fetch_number_value(intr, &current -> val_b, current -> is_b_immediate)) {
                    intr -> is_greater = true;
                } else if ((uint64_t) fetch_number_value(intr, &current -> val_a, false) < (uint64_t) fetch_number_value(intr, &current -> val_b, current -> is_b_immediate)) {
                    intr -> is_less = true;
                } else {
                    intr -> is_equal = true;
                }
                current = current->next;
                break;
            case CMD_PRINT:
                print_base(intr, current);
                current = current->next;
                break;
            case CMD_AND:
                intr -> variables[current -> destination.num_val] = fetch_number_value(intr, &current -> val_a, false) 
                    & fetch_number_value(intr, &current -> val_b, false);
                current = current->next;
                break;
            case CMD_ORR:
                intr -> variables[current -> destination.num_val] = fetch_number_value(intr, &current -> val_a, false) 
                    | fetch_number_value(intr, &current -> val_b, false);
                current = current->next;
                break;
            case CMD_EOR:
                intr -> variables[current -> destination.num_val] = fetch_number_value(intr, &current -> val_a, false) 
                    ^ fetch_number_value(intr, &current -> val_b, false);
                current = current->next;
                break;
            case CMD_LSL:
                intr -> variables[current -> destination.num_val] = (uint64_t) fetch_number_value(intr, &current -> val_a, false) 
                    << (uint64_t) fetch_number_value(intr, &current -> val_b, true);
                current = current->next;
                break;
            case CMD_LSR:
                intr -> variables[current -> destination.num_val] = (uint64_t) fetch_number_value(intr, &current -> val_a, false) 
                    >> (uint64_t) fetch_number_value(intr, &current -> val_b, true);
                current = current->next;
                break; 
            case CMD_ASR:
                intr -> variables[current -> destination.num_val] = fetch_number_value(intr, &current -> val_a, false) 
                    >> fetch_number_value(intr, &current -> val_b, true);
                current = current->next;
                break;      
            case CMD_LOAD: {
                int64_t num = 0;
                if (!mem_load((uint8_t *) &num, fetch_number_value(intr, &current -> val_b, current -> is_b_immediate), 
                    fetch_number_value(intr, &current -> val_a, true))) {
                        intr -> had_error = true;
                    } 
                intr -> variables[current -> destination.num_val] = num;  
                current = current -> next;    
                break;    
            }
            case CMD_STORE:
                if (!mem_store((uint8_t *) &intr -> variables[current -> destination.num_val], fetch_number_value(intr, &current -> val_a, current -> is_a_immediate), 
                    fetch_number_value(intr, &current -> val_b, true))) {
                        intr -> had_error = true;
                    }
                current = current->next;
                break;
            case CMD_PUT: {
                char *charArray = current->destination.str_val;
                int count = 0;
                while (*charArray != '\0') {
                    if (!mem_store((uint8_t *) charArray, fetch_number_value(intr, &current->val_a, current->is_a_immediate) + count, 1)) {
                        intr->had_error = true;
                        break;
                    }
                    count++;
                    charArray++;
                }
                if (!mem_store((uint8_t *) charArray, fetch_number_value(intr, &current->val_a, current->is_a_immediate) + count, 1)) {
                    intr->had_error = true;
                    ufree(current->destination.str_val);
                    break;
                }
                ufree(current -> destination.str_val);
                current = current->next;
                break;
            }
            case CMD_BRANCH:
                if (cond_holds(intr, current -> branch_condition)) {
                    Entry * ent = get_label(intr -> label_map, current -> destination.str_val);
                    if (ent == NULL) {
                        printf("Label not found: ");
                        printf("%s\n", current -> destination.str_val);
                        intr -> had_error = true;
                        return;
                    }
                    current = ent -> command;
                } else {
                    current = current -> next;
                }
                break;
            case CMD_CALL: {
                StackEntry* se = umalloc(sizeof(StackEntry));
                if (!se) {
                    intr->had_error = true;
                    return;
                }
                Entry* ent = get_label(intr->label_map, current->destination.str_val);
                if (ent != NULL && ent->command != NULL) {
                    se->command = current;
                } else {
                    printf("Label not found: %s\n", current->destination.str_val);
                    intr->had_error = true;
                    ufree(se);
                    return;
                }
                for (int i = 0; i < 32; i++) {
                    se->variables[i] = intr->variables[i];
                }
                // Always initialize the next pointer, regardless of the current stack state.
                se->next = intr->the_stack;
                intr->the_stack = se;
                current = ent->command;
                break;
            }
            case CMD_RET:
                if (intr -> the_stack == NULL) {
                    current = NULL;
                    break;
                } else {
                    StackEntry* temp = intr -> the_stack;
                    current = intr -> the_stack -> command;
                    for (int i = 1; i < 32; i++) {
                        intr -> variables[i] = intr -> the_stack -> variables[i];
                    }                
                    intr -> the_stack = intr -> the_stack -> next;
                    ufree(temp);  // Free the allocated memory for StackEntry
                }
                current = current -> next;
                break;
                default:
                    intr -> had_error = true;
                    break;
        }          
    }

    // Week 4: free the stack at the end
    while (intr->the_stack != NULL) {
        StackEntry *temp = intr->the_stack;
        intr->the_stack  = intr->the_stack->next;
        ufree(temp);
    }
}

void print_interpreter_state(Interpreter *intr) {
    if (!intr) {
        return;
    }

    printf("Error: %d\n", intr->had_error);
    printf("Flags:\n");
    printf("Is greater: %d\n", intr->is_greater);
    printf("Is equal: %d\n", intr->is_equal);
    printf("Is less: %d\n", intr->is_less);

    printf("\n");

    printf("Variable values:\n");
    for (size_t i = 0; i < NUM_VARIABLES; i++) {
        printf("x%zu: %" PRId64 "", i, intr->variables[i]);

        if (i < NUM_VARIABLES - 1) {
            printf(", ");
        }

        if ((i + 1) % 8 == 0) {
            printf("\n");
        }
    }

    printf("\n");
}

/**
 * @brief Fetches the appropriate value from the given operand.
 *
 * @param intr The pointer to the interpreter holding variable state.
 * @param op The operand used to fetch the value.
 * @param is_im A boolean representing whether this value is an immediate or
 * must be read in from the interpreter state.
 * @return The fetched value.
 */
static int64_t fetch_number_value(Interpreter *intr, Operand *op, bool is_im) {
    // STUDENT TODO: Fetch either a variable from the interpreter's state or directly output a value
    if (is_im) {
        return op -> num_val;
    } else {
        return intr -> variables[op -> num_val];
    }
}

/**
 * @brief Determines whether a given branch condition holds.
 *
 * @param intr The pointer to the interpreter holding the result of the
 * comparison.
 * @param cond The condition to check.
 * @return True if the given condition holds, false otherwise.
 */
static bool cond_holds(Interpreter *intr, BranchCondition cond) {
    // STUDENT TODO: Determine whether a given condition holds using the interpreter's state
    if (cond == BRANCH_ALWAYS) {
        return true;
    } else if (cond == BRANCH_EQUAL) {
        if (intr -> is_equal) {
            return true;
        }
    } else if (cond == BRANCH_GREATER) {
        if (intr -> is_greater) {
            return true;
        }
    } else if (cond == BRANCH_GREATER_EQUAL) {
        if (intr -> is_greater || intr -> is_equal) {
            return true;
        }
    } else if (cond == BRANCH_LESS) {
        if (intr -> is_less) {
            return true;
        }
    } else if (cond == BRANCH_LESS_EQUAL) {
        if (intr -> is_less || intr -> is_equal) {
            return true;
        }
    } else if (cond == BRANCH_NOT_EQUAL) {
        if (!intr -> is_equal) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Prints the given command's value in a specified base.
 *
 * @param intr The pointer to the interpreter holding variable state.
 * @param cmd The command being processed.
 * @return True whether the print was successful, false otherwise.
 */
static bool print_base(Interpreter *intr, Command *cmd) {
    // STUDENT TODO: Print the given value respecting the appropriate base
    int64_t varOrImm;
    if (cmd -> is_a_immediate) {
        varOrImm = cmd -> val_a.num_val;
    } else {
        varOrImm = intr -> variables[cmd -> val_a.num_val];
    }
    if (cmd -> val_b.base == 'd') {
        printf("%" PRId64 "\n", varOrImm);
        return true;
    } else if (cmd -> val_b.base == 'x') {
        printf("0x%lx\n", (uint64_t) varOrImm);
        return true;
    } else if (cmd -> val_b.base == 'b') {
        int numDigits[64];
        int maxIndices = 0;
        uint64_t num = varOrImm;
        while (num > 0) {
            numDigits[maxIndices] = num % 2;
            num = num / 2;
            maxIndices++;
        }
        printf("0b");
        if (maxIndices == 0) {
            printf("0\n");
            return true;
        }
        for (int i = maxIndices - 1; i >= 0; i--) {
            printf("%d", numDigits[i]);
        }
        printf("\n");
        return true;
    } else if (cmd -> val_b.base == 's') {
        char character = 0;
        int count = 0;
        while (true) {
            if (mem_load((uint8_t *) &character, varOrImm + count, 1)) {
                if (character == 0) {
                    break;                    
                }
                printf("%c", character);
            } else {
                return false;
            }
            if (character == 0) {
                break;                
            }
            count++;
        }
        printf("\n");
        return true;
    }
    return false;
}