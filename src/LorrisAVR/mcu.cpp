/****************************************************************************
**
**    This file is part of Lorris.
**    Copyright (C) 2012 Vojtěch Boček
**
**    Contact: <vbocek@gmail.com>
**             https://github.com/Tasssadar
**
**    Lorris is free software: you can redistribute it and/or modify
**    it under the terms of the GNU General Public License as published by
**    the Free Software Foundation, either version 3 of the License, or
**    (at your option) any later version.
**
**    Lorris is distributed in the hope that it will be useful,
**    but WITHOUT ANY WARRANTY; without even the implied warranty of
**    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**    GNU General Public License for more details.
**
**    You should have received a copy of the GNU General Public License
**    along with Lorris.  If not, see <http://www.gnu.org/licenses/>.
**
****************************************************************************/

#include <QDebug>
#include <stdio.h>

#define DEBUG 1

#include "mcu.h"
#include "instructions.h"
#include "mcu_prototype.h"

MCU::MCU() : QThread()
{
    m_freq = 20000000 / 2; // 20MHz
    m_protype = &atmega328p;

    m_data_mem = m_prog_mem = m_eeprom = NULL;
    m_data_section = m_bss_section = NULL;

    m_run = true;
}

MCU::~MCU()
{
    m_run = false;
    wait();

    for(quint32 i = 0; i < m_protype->prog_mem_size; ++i)
        delete m_instructions[i];
    delete[] m_instructions;

    delete[] m_data_mem;
    delete[] m_prog_mem;
    delete[] m_eeprom;
}

void MCU::init(HexFile *hex)
{
    // Allocate memory
    m_data_mem = new quint8[m_protype->sram_size + 0xFF];
    m_prog_mem = new quint8[m_protype->prog_mem_size];
    m_eeprom = new quint8[m_protype->eeprom_size];

    m_data_section = m_data_mem + 0x100;

    // set stack pointer to byte behind sram end
    // program should do this by itself
    m_stack_pointer = wrapper_16(m_data_mem + m_protype->SPL);
    m_stack_pointer.set(m_protype->sram_size + 0xFF);

    x_register = wrapper_16(m_data_mem + 0x1A);
    y_register = wrapper_16(m_data_mem + 0x1C);
    z_register = wrapper_16(m_data_mem + 0x1E);

    m_sreg = m_data_mem + 0x5F;
    *m_sreg = 0;

    m_program_counter = 0x00;

    m_instructions = new instruction*[m_protype->prog_mem_size];
    for(quint32 i = 0; i < m_protype->prog_mem_size; ++i)
        m_instructions[i] = NULL;

    addInstHandlers();

    HexFile::regionMap& data = hex->getData();
    bool load_data = false;
    for(HexFile::regionMap::iterator itr = data.begin(); itr != data.end(); ++itr)
    {
        quint32 offset = itr->first;

        std::vector<quint8>& sec_data = itr->second;

        // load instructions
        quint32 i = 0;
        while(!load_data && i+1 < sec_data.size())
        {
            quint8 first = sec_data[i];
            quint8 second = sec_data[i+1];

            m_prog_mem[offset + i] = first;
            m_prog_mem[offset + i+1] = second;

            quint16 inst_num = (second << 8) | first;
            inst_prototype *prot = getInstPrototype(inst_num);

            quint16 next_inst = 0;
            if(prot->words == 2)
            {
                Q_ASSERT(sec_data.size() > i+3);
                next_inst = (sec_data[i+3] << 8) | sec_data[i+2];
                m_prog_mem[offset + i+2] = sec_data[i+2];
                m_prog_mem[offset + i+3] = sec_data[i+3];
            }

            if(!prot)
            {
                //Q_ASSERT?
                qDebug() << "Unhandled instruction " << inst_num << second << first;
                i += 2;
                continue;
            }
            int arg1 = arg_resolvers[prot->arg1](inst_num, next_inst, offset+i);
            int arg2 = arg_resolvers[prot->arg2](inst_num, next_inst, offset+i);

            instruction *inst = new instruction;
            inst->arg1 = arg1;
            inst->arg2 = arg2;
            inst->handler = getInstHandler(prot->id);
            inst->prototype = prot;

            m_instructions[offset+i] = inst;
            i += prot->words*2;

            // FIXME: I really dont know how .data and .text are
            // splited, buw I think .text section always ends with rjmp .-2
            if(prot->id == 101 && inst->arg1 == -2) // rjmp   .-2
                load_data = true;
        }

        // load data
        // FIXME: is it 16bit aligned?
        while(load_data && i+1 < sec_data.size())
        {
            m_data_section[offset + i] = sec_data[i];
            m_data_section[offset + i+1] = sec_data[i+1];
            i += 2;
        }
    }
#if DEBUG
    for(quint32 i = 0; i < m_protype->prog_mem_size; ++i)
    {
        if(!m_instructions[i])
            continue;

        instruction *inst = m_instructions[i];

        char buf[512];

        int name_len = 8 - strlen(inst->prototype->name);

        if(inst->prototype->arg2 != NONE)
             ::snprintf(buf, 512, "0x%04x: %s%*s0x%02x 0x%02x", i, inst->prototype->name, name_len, "", inst->arg1, inst->arg2);
        else if(inst->prototype->arg1 != NONE)
        {
            if(!strcmp("rcall", inst->prototype->name) | !strcmp("rjmp", inst->prototype->name))
                ::snprintf(buf, 512, "0x%04x: %s%*s.%d", i, inst->prototype->name, name_len, "", inst->arg1);
            else
                ::snprintf(buf, 512, "0x%04x: %s%*s0x%02x", i, inst->prototype->name, name_len, "", inst->arg1);
        }
        else
            ::snprintf(buf, 512, "0x%04x: %s", i, inst->prototype->name);


        qDebug() << QString::fromAscii(buf);
    }
#endif
}

inst_prototype *MCU::getInstPrototype(quint16 val)
{
    for(inst_prototype *p = instructions; p->name; ++p)
        if((val & p->mask) == p->opcode)
            return p;
    return NULL;
}

void MCU::startMCU()
{
    start();

    m_cycles_debug = 0;
    m_cycles_debug_counter = 0;
    connect(&m_cycles_timer, SIGNAL(timeout()), SLOT(checkCycles()));
    m_cycles_timer.start(5000);
}

void MCU::run()
{
    m_cycle_counter = 0;
    instruction *inst = NULL;

    while(m_run)
    {
        inst = m_instructions[m_program_counter];

        Q_ASSERT(inst);

        m_program_counter += (inst->prototype->words*2);

        if(inst->handler == NULL)
            qDebug("No handler for ints %s", inst->prototype->name);
        else
            cycle_sleep = ((*this).*inst->handler)(inst->arg1, inst->arg2);

        //for(;cycle_sleep != 0; --cycle_sleep)
        {
            usleep(1);
            //for(quint32 i = 0; i < 500; ++i)
            //    __asm__ volatile ("nop");
            ++m_cycle_counter;
        }
    }
}

void MCU::checkCycles()
{
    //QMutexLocker l(&m_counter_mutex);

    qDebug("Freq: %u %u", m_cycle_counter/5, ((m_data_mem[y_register.get()+1] << 8) | m_data_mem[y_register.get()]));
    m_cycle_counter = 0;
}

MCU::instHandler MCU::getInstHandler(quint8 id)
{
    if(m_handlers.contains(id))
        return m_handlers[id];
    return NULL;
}

void MCU::addInstHandlers()
{
    m_handlers.insert(2,   &MCU::__adiw);
    m_handlers.insert(6,   &MCU::__bclr);
    m_handlers.insert(31,  &MCU::__call);
    m_handlers.insert(55,  &MCU::__eor);
    m_handlers.insert(63,  &MCU::__jmp);
    m_handlers.insert(76,  &MCU::__ldd_y_plus);
    m_handlers.insert(78,  &MCU::__ldi);
    m_handlers.insert(61,  &MCU::__in);
    m_handlers.insert(95,  &MCU::__out);
    m_handlers.insert(97,  &MCU::__push);
    m_handlers.insert(98,  &MCU::__rcall);
    m_handlers.insert(101, &MCU::__rjmp);
    m_handlers.insert(130, &MCU::__std_y_plus);
}

void MCU::setDataMem16(int idx, quint16 val)
{
    m_data_mem[idx++] = (val >> 8);
    m_data_mem[idx] = (val & 0x0F);
}

void MCU::check_ZNS(quint8 res)
{
    quint8 val = *m_sreg;
    if(!res)
        val |= SREG_ZERO;
    else
        val &= ~(SREG_ZERO);

    if(res & 0x80)
        val |= SREG_NEG;
    else
        val &= ~(SREG_NEG);

    if(bool(val & SREG_NEG) ^ bool(val & SREG_V))
        val |= SREG_SIGN;
    else
        val &= ~(SREG_SIGN);

    *m_sreg = val;
}

quint8 MCU::__adiw(int arg1, int arg2)
{
    wrapper_16 wrap(m_data_mem + arg1);
    wrap += arg2;
    return 2;
}

quint8 MCU::__bclr(int arg1, int /*arg2*/)
{
    *m_sreg &= ~(1 << arg1);
    return 1;
}

quint8 MCU::__call(int arg1, int /*arg2*/)
{
    if(m_protype->bit16)
    {
        m_stack_pointer -= 2;
        setDataMem16(m_stack_pointer.get(), m_program_counter);
        m_program_counter = arg1;
        return 4;
    }
    else
    {
        m_stack_pointer -= 3;
        m_data_mem[m_stack_pointer.get()] = (m_program_counter >> 16);
        m_data_mem[m_stack_pointer.get()+1] = (m_program_counter >> 8);
        m_data_mem[m_stack_pointer.get()+2] = (m_program_counter & 0xF);
        m_program_counter = arg1;
        return 5;
    }
}

quint8 MCU::__eor(int arg1, int arg2)
{
    m_data_mem[arg1] ^= m_data_mem[arg2];

    *m_sreg &= ~(SREG_V);
    check_ZNS(m_data_mem[arg1]);
    return 1;
}

quint8 MCU::__jmp(int arg1, int /*arg2*/)
{
    m_program_counter = arg1;
    return 3;
}

quint8 MCU::__ldd_y_plus(int arg1, int arg2)
{
    m_data_mem[arg1] = m_data_mem[y_register.get()+arg2];
    return 3;
}

quint8 MCU::__ldi(int arg1, int arg2)
{
    m_data_mem[arg1] = arg2;
    return 1;
}

quint8 MCU::__in(int arg1, int arg2)
{
    m_data_mem[arg2] = m_data_mem[arg1];
    return 1;
}

quint8 MCU::__out(int arg1, int arg2)
{
    m_data_mem[arg1] = m_data_mem[arg2];
    return 1;
}

quint8 MCU::__push(int arg1, int /*arg2*/)
{
    --m_stack_pointer;
    m_data_mem[m_stack_pointer.get()] = m_data_mem[arg1];
    return 2;
}

quint8 MCU::__rcall(int arg1, int /*arg2*/)
{
    if(m_protype->bit16)
    {
        m_stack_pointer -= 2;
        setDataMem16(m_stack_pointer.get(), m_program_counter);
        m_program_counter += arg1;
        return 3;
    }
    else
    {
        m_stack_pointer -= 3;
        m_data_mem[m_stack_pointer.get()] = (m_program_counter >> 16);
        m_data_mem[m_stack_pointer.get()+1] = (m_program_counter >> 8);
        m_data_mem[m_stack_pointer.get()+2] = (m_program_counter & 0xF);
        m_program_counter += arg1;
        return 4;
    }
}

quint8 MCU::__rjmp(int arg1, int /*arg2*/)
{
    m_program_counter += arg1;
    return 2;
}

quint8 MCU::__std_y_plus(int arg1, int arg2)
{
    m_data_mem[y_register.get()+arg1] = m_data_mem[arg2];
    return 2;
}
