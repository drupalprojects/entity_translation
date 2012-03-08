<?php

/**
 * @file
 * Node translation handler for the entity translation module.
 */


/**
 * Node translation handler.
 *
 * Overrides default behaviours for Node properties.
 */
class EntityTranslationNodeHandler extends EntityTranslationDefaultHandler {

  public function __construct($entity_type, $entity_info, $entity, $entity_id) {
    parent::__construct('node', $entity_info, $entity, $entity_id);
  }

  public function isRevision() {
    return !empty($this->entity->revision);
  }

  public function getLanguage() {
    return $this->entity->language;
  }

  public function getAccess($op) {
    return node_access($op, $this->entity);
  }

  /**
   * Perform alterations on the node edit form.
   *
   * Clean up the language selector to avoid the possibility to change the node
   * language to a value already assigned to an existing translation.
   * Convert the translation update status fieldset into a vartical tab.
   */
  public function entityForm(&$form, &$form_state) {
    parent::entityForm($form, $form_state);

    // For translations update the page title to match the "original" one.
    if ($this->isTranslationForm()) {
      $languages = language_list();
      $form_langcode = $this->getFormLanguage();
      $type_name = node_type_get_name($this->entity);
      $t_args = array('@type' => $type_name, '@title' => $this->getLabel(), '@language' => t($languages[$form_langcode]->name));
      drupal_set_title(t('<em>Edit @type</em> @title [@language translation]', $t_args), PASS_THROUGH);
    }

    // Disable languages for existing translations, so it is not possible to
    // switch this node to some language which is already in the translation
    // set.
    $translations = $this->getTranslations();
    foreach ($translations->data as $langcode => $translation) {
      if ($langcode != $translations->original) {
        unset($form['language']['#options'][$langcode]);
      }
    }
    if (count($translations->data) > 1) {
      unset($form['language']['#options'][LANGUAGE_NONE]);
    }

    // Move the translation fieldset to a vertical tab.
    if (isset($form['translation'])) {
      $form['translation'] += array(
        '#group' => 'additional_settings',
        '#weight' => 100,
        '#attached' => array(
          'js' => array(drupal_get_path('module', 'entity_translation') . '/entity_translation.node-form.js'),
        ),
      );
    }
  }

  public function entityFormSubmit($form, &$form_state) {
    if (!isset($form_state['values']['translation'])) {
      $form_state['values']['translation'] = array('status' => $form_state['values']['status']);
    }
    $values = &$form_state['values']['translation'];

    if (!$this->isTranslationForm()) {
      // Inherit entity authoring information for the original values.
      $values['name'] = $form_state['values']['name'];
      if (!empty($form_state['values']['date'])) {
        $values['created'] = $form_state['values']['date'];
      }
    }

    parent::entityFormSubmit($form, $form_state);
  }

  protected function getStatus() {
    return (boolean) $this->entity->status;
  }
}
