<?php

/**
 * @file
 * Comment translation handler for the entity translation module.
 */


/**
 * Comment translation handler.
 */
class EntityTranslationCommentHandler extends EntityTranslationDefaultHandler {

  public function __construct($entity_type, $entity_info, $entity, $entity_id) {
    parent::__construct('comment', $entity_info, $entity, $entity_id);
  }

  public function getLanguage() {
    return $this->entity->language;
  }

  public function entityForm(&$form, &$form_state) {
    parent::entityForm($form, $form_state);

    // For translations update the page title to match the "original" one.
    if ($this->isTranslationForm()) {
      $languages = language_list();
      $form_langcode = $this->getFormLanguage();
      $t_args = array('@subject' => $this->getLabel(), '@language' => t($languages[$form_langcode]->name));
      drupal_set_title(t('Edit comment @subject [@language translation]', $t_args), PASS_THROUGH);
    }

    // Adjust the translation fieldset weight to move it below the admin one.
    $form['translation']['#weight'] = 1;
  }

  protected function getStatus() {
    return (boolean) $this->entity->status;
  }
}
