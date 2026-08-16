-- Impedisce al player di saltare (Space / pad A)
-- Uso in Studio:
--   1) Apri Script globale / place script, incolla questo
--   2) oppure Properties di una Part → Script → incolla
--   3) premi Test (F5) — Space non fa più saltare
--
-- Per riattivare: UserInputService.EnableJump()
--               oppure Humanoid:SetJumpEnabled(true)

print("[no_jump] Salto disattivato")

UserInputService.DisableJump()

-- Alternativa equivalente:
-- Humanoid:SetJumpEnabled(false)
