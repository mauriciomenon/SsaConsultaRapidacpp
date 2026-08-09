#pragma once

#include <array>
#include <string_view>

namespace ssa::domain {

    enum class SamOrganizationGroup {
        MaintenanceSuperintendence,
        MaintenanceEngineering,
        Maintenance,
        OperationsSuperintendence,
        SystemOperations,
        PlantOperations,
    };

    struct SamOrganizationalUnit final {
        std::string_view code;
        std::string_view name;
        SamOrganizationGroup group;
    };

    inline constexpr auto kSamOrganizationalUnits = std::array{
        SamOrganizationalUnit{"SMA0", "Superintendencia de Manutencao",
                              SamOrganizationGroup::MaintenanceSuperintendence},
        SamOrganizationalUnit{"IDE0", "Engenharia de Manutencao - Departamento",
                              SamOrganizationGroup::MaintenanceEngineering},
        SamOrganizationalUnit{"IEE0", "Engenharia de Manutencao Eletronica",
                              SamOrganizationGroup::MaintenanceEngineering},
        SamOrganizationalUnit{"IEE1", "Engenharia de Manutencao Eletronica - Protecao",
                              SamOrganizationGroup::MaintenanceEngineering},
        SamOrganizationalUnit{"IEE2", "Engenharia de Manutencao Eletronica - Regulacao",
                              SamOrganizationGroup::MaintenanceEngineering},
        SamOrganizationalUnit{"IEE3", "Engenharia de Manutencao Eletronica - Comunicacao",
                              SamOrganizationGroup::MaintenanceEngineering},
        SamOrganizationalUnit{"IEE4", "Engenharia de Manutencao Eletronica - Sistemas Digitais",
                              SamOrganizationGroup::MaintenanceEngineering},
        SamOrganizationalUnit{"IEQ0", "Engenharia de Manutencao Eletrica",
                              SamOrganizationGroup::MaintenanceEngineering},
        SamOrganizationalUnit{"IEQ1", "Engenharia de Manutencao Eletrica - Geradores",
                              SamOrganizationGroup::MaintenanceEngineering},
        SamOrganizationalUnit{"IEQ2", "Engenharia de Manutencao Eletrica - Servicos Auxiliares",
                              SamOrganizationGroup::MaintenanceEngineering},
        SamOrganizationalUnit{"IEQ3", "Engenharia de Manutencao Eletrica - Alta Tensao",
                              SamOrganizationGroup::MaintenanceEngineering},
        SamOrganizationalUnit{"ILA0", "Engenharia de Manutencao Laboratorio",
                              SamOrganizationGroup::MaintenanceEngineering},
        SamOrganizationalUnit{"ILA1", "Engenharia de Manutencao Laboratorio - Ensaios",
                              SamOrganizationGroup::MaintenanceEngineering},
        SamOrganizationalUnit{"ILA2", "Engenharia de Manutencao Laboratorio - Instrumentacao",
                              SamOrganizationGroup::MaintenanceEngineering},
        SamOrganizationalUnit{"ILA3", "Engenharia de Manutencao Laboratorio - Padroes",
                              SamOrganizationGroup::MaintenanceEngineering},
        SamOrganizationalUnit{"ILA4", "Engenharia de Manutencao Laboratorio - Quimica",
                              SamOrganizationGroup::MaintenanceEngineering},
        SamOrganizationalUnit{"IMA0", "Engenharia de Manutencao Materiais",
                              SamOrganizationGroup::MaintenanceEngineering},
        SamOrganizationalUnit{"IME0", "Engenharia de Manutencao Mecanica",
                              SamOrganizationGroup::MaintenanceEngineering},
        SamOrganizationalUnit{
            "IME1", "Engenharia de Manutencao Mecanica - Regulador de Velocidade / Hidromecanicos",
            SamOrganizationGroup::MaintenanceEngineering},
        SamOrganizationalUnit{"IME2", "Engenharia de Manutencao Mecanica - Turbinas / Auxiliares",
                              SamOrganizationGroup::MaintenanceEngineering},
        SamOrganizationalUnit{
            "IME3",
            "Engenharia de Manutencao Mecanica - Geradores / Equipamentos de Elevacao e Transporte",
            SamOrganizationGroup::MaintenanceEngineering},
        SamOrganizationalUnit{"ISI0", "Engenharia de Manutencao Sistematizacao",
                              SamOrganizationGroup::MaintenanceEngineering},
        SamOrganizationalUnit{"ISI1", "Engenharia de Manutencao Civil",
                              SamOrganizationGroup::MaintenanceEngineering},
        SamOrganizationalUnit{"MAS0", "Departamento de Manutencao - Assessoria",
                              SamOrganizationGroup::Maintenance},
        SamOrganizationalUnit{"MAM0", "Manutencao Auxiliares Mecanicos - Programacao",
                              SamOrganizationGroup::Maintenance},
        SamOrganizationalUnit{"MAM1",
                              "Manutencao Auxiliares Mecanicos - Estacoes de Bombeamento / Redes "
                              "de Agua / Anti-Incendio",
                              SamOrganizationGroup::Maintenance},
        SamOrganizationalUnit{"MAM2",
                              "Manutencao Auxiliares Mecanicos - Vertedouro / Rede de Ar / Diesel "
                              "/ Equipamentos de Elevacao e Transporte",
                              SamOrganizationGroup::Maintenance},
        SamOrganizationalUnit{"MAM3",
                              "Manutencao Auxiliares Mecanicos - Ventilacao / Ar-Condicionado",
                              SamOrganizationGroup::Maintenance},
        SamOrganizationalUnit{"MCI0", "Manutencao Civil Industrial - Programacao",
                              SamOrganizationGroup::Maintenance},
        SamOrganizationalUnit{"MCI1", "Manutencao Civil Industrial - Manutencao Civil",
                              SamOrganizationGroup::Maintenance},
        SamOrganizationalUnit{"MCI2", "Manutencao Civil Industrial - Apoio a Manutencao",
                              SamOrganizationGroup::Maintenance},
        SamOrganizationalUnit{"MCI3", "Manutencao Civil Industrial - Contratos e Planejamento",
                              SamOrganizationGroup::Maintenance},
        SamOrganizationalUnit{"MCI4", "Manutencao Civil Industrial - Protecao Anticorrosiva",
                              SamOrganizationGroup::Maintenance},
        SamOrganizationalUnit{"MEG0", "Manutencao Equipamentos de Geracao - Programacao",
                              SamOrganizationGroup::Maintenance},
        SamOrganizationalUnit{"MEG1", "Manutencao Equipamentos de Geracao - Gerador e Agregados",
                              SamOrganizationGroup::Maintenance},
        SamOrganizationalUnit{"MEG2",
                              "Manutencao Equipamentos de Geracao - Equipamentos de Baixa Tensao",
                              SamOrganizationGroup::Maintenance},
        SamOrganizationalUnit{"MEG3", "Manutencao Equipamentos de Geracao - Servicos Auxiliares",
                              SamOrganizationGroup::Maintenance},
        SamOrganizationalUnit{"MEL0", "Manutencao Eletronica - Programacao",
                              SamOrganizationGroup::Maintenance},
        SamOrganizationalUnit{"MEL1", "Manutencao Eletronica - Protecao e Controle",
                              SamOrganizationGroup::Maintenance},
        SamOrganizationalUnit{"MEL2", "Manutencao Eletronica - Regulacao",
                              SamOrganizationGroup::Maintenance},
        SamOrganizationalUnit{"MEL3", "Manutencao Eletronica - Comunicacoes",
                              SamOrganizationGroup::Maintenance},
        SamOrganizationalUnit{"MEL4", "Manutencao Eletronica - Sistemas Digitais",
                              SamOrganizationGroup::Maintenance},
        SamOrganizationalUnit{"MET0", "Manutencao Equipamentos de Transmissao - Programacao",
                              SamOrganizationGroup::Maintenance},
        SamOrganizationalUnit{"MET1", "Manutencao Equipamentos de Transmissao - Transformadores",
                              SamOrganizationGroup::Maintenance},
        SamOrganizationalUnit{
            "MET2", "Manutencao Equipamentos de Transmissao - Equipamentos de Manobra e Linhas",
            SamOrganizationGroup::Maintenance},
        SamOrganizationalUnit{"MET3", "Manutencao Equipamentos de Transmissao - GIS",
                              SamOrganizationGroup::Maintenance},
        SamOrganizationalUnit{"MMU0", "Manutencao Mecanica de Unidades - Programacao",
                              SamOrganizationGroup::Maintenance},
        SamOrganizationalUnit{"MMU1", "Manutencao Mecanica de Unidades - Gerador e Agregados",
                              SamOrganizationGroup::Maintenance},
        SamOrganizationalUnit{
            "MMU2", "Manutencao Mecanica de Unidades - Turbina / Equipamentos Hidromecanicos",
            SamOrganizationGroup::Maintenance},
        SamOrganizationalUnit{"MMU3", "Manutencao Mecanica de Unidades - Oficina Eletromecanica",
                              SamOrganizationGroup::Maintenance},
        SamOrganizationalUnit{"SOA0", "Superintendencia de Operacao - Assessoria",
                              SamOrganizationGroup::OperationsSuperintendence},
        SamOrganizationalUnit{"OSA0", "Operacao do Sistema - Assessoria",
                              SamOrganizationGroup::SystemOperations},
        SamOrganizationalUnit{"OSE0", "Operacao do Sistema - Estudos Eletricos",
                              SamOrganizationGroup::SystemOperations},
        SamOrganizationalUnit{"OSH0", "Operacao do Sistema - Hidrologia",
                              SamOrganizationGroup::SystemOperations},
        SamOrganizationalUnit{"OSH1", "Operacao do Sistema - Hidrologia de Campo",
                              SamOrganizationGroup::SystemOperations},
        SamOrganizationalUnit{"OSO0", "Operacao do Sistema - Operacao",
                              SamOrganizationGroup::SystemOperations},
        SamOrganizationalUnit{"OSO1", "Operacao do Sistema - Despacho de Carga",
                              SamOrganizationGroup::SystemOperations},
        SamOrganizationalUnit{"OPS0", "Operacao do Sistema - Programacao e Estatistica",
                              SamOrganizationGroup::SystemOperations},
        SamOrganizationalUnit{"OUE0", "Estudos / Normas / Programacao / Estatistica - Operacao",
                              SamOrganizationGroup::PlantOperations},
        SamOrganizationalUnit{"OUE1", "Estudos / Normas / Programacao / Estatistica - Pre-operacao",
                              SamOrganizationGroup::PlantOperations},
        SamOrganizationalUnit{"OUE2", "Estudos / Normas / Programacao / Estatistica - Pos-operacao",
                              SamOrganizationGroup::PlantOperations},
        SamOrganizationalUnit{"OUE3",
                              "Estudos / Normas / Programacao / Estatistica - Normas e Estudos",
                              SamOrganizationGroup::PlantOperations},
        SamOrganizationalUnit{"OUE4",
                              "Estudos / Normas / Programacao / Estatistica - ETA / Comunicacao",
                              SamOrganizationGroup::PlantOperations},
        SamOrganizationalUnit{"OUO0", "Operacao da Usina e Subestacoes - Operacao",
                              SamOrganizationGroup::PlantOperations},
        SamOrganizationalUnit{"OUO1", "Operacao da Usina e Subestacoes - Assessoria Turno 50 Hz",
                              SamOrganizationGroup::PlantOperations},
        SamOrganizationalUnit{"OUO2", "Operacao da Usina e Subestacoes - Assessoria Turno 60 Hz",
                              SamOrganizationGroup::PlantOperations},
        SamOrganizationalUnit{
            "OUO3", "Operacao da Usina e Subestacoes - Revisao / Ensaios de Unidades Geradoras",
            SamOrganizationGroup::PlantOperations},
        SamOrganizationalUnit{"OUO4", "Operacao da Usina e Subestacoes - Operacao em Tempo Real",
                              SamOrganizationGroup::PlantOperations},
        SamOrganizationalUnit{"OUO5", "Operacao da Usina e Subestacoes - Turno Setor 50 Hz",
                              SamOrganizationGroup::PlantOperations},
        SamOrganizationalUnit{"OUO6", "Operacao da Usina e Subestacoes - Turno Setor 60 Hz",
                              SamOrganizationGroup::PlantOperations},
        SamOrganizationalUnit{"OUO7", "Operacao da Usina e Subestacoes - Turno SEMD",
                              SamOrganizationGroup::PlantOperations},
    };

    [[nodiscard]] inline constexpr const SamOrganizationalUnit*
    samOrganizationalUnit(const std::string_view code) noexcept {
        for (const auto& unit : kSamOrganizationalUnits) {
            if (unit.code == code) {
                return &unit;
            }
        }
        return nullptr;
    }

    [[nodiscard]] inline constexpr std::string_view
    samOrganizationGroupName(const SamOrganizationGroup group) noexcept {
        switch (group) {
        case SamOrganizationGroup::MaintenanceSuperintendence:
            return "Superintendencia de Manutencao (SM.DT)";
        case SamOrganizationGroup::MaintenanceEngineering:
            return "Departamento de Engenharia de Manutencao (SMI.DT)";
        case SamOrganizationGroup::Maintenance:
            return "Departamento de Manutencao (SMM.DT)";
        case SamOrganizationGroup::OperationsSuperintendence:
            return "Superintendencia de Operacao (OP.DT)";
        case SamOrganizationGroup::SystemOperations:
            return "Operacao do Sistema (OPS.DT)";
        case SamOrganizationGroup::PlantOperations:
            return "Operacao da Usina e Subestacoes (OPU.DT)";
        }
        return {};
    }

} // namespace ssa::domain
